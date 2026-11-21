# Parallel recording by blocks: clean-room spec (W4)

Status: spec. Written on 2026-11-19 from reading the rehooked repository (`src/gpu/vk_gpu.cpp`,
"Parallel recording, retirement and submission", and `renderer_impl.h`). It describes behavior
only: no code, names or structure are copied, and the implementation is written without opening
that repository again (roadmap W4 protocol).

## What the other renderer does (observed behavior)

1. It renders a whole guest submission at once, after the guest submitted it: the command stream
   is complete before recording starts.
2. One serial pass walks the stream. It applies register writes, records by itself the work that
   orders what follows (dispatches, draws into linear targets, queries the CPU reads back), and
   cuts the remaining draws into blocks.
3. Block size: about a quarter of one thread's share of the previous frame's recording time,
   measured per draw (64 draws when nothing is known yet); a cut prefers a draw where the render
   targets change, and is forced at twice that size.
4. Each block starts from a saved copy of the registers at its first draw, so any thread can
   record it without the walk.
5. N recording threads, the submitting one included, take blocks in any order and record each into
   its own secondary command buffer. Each block keeps its own dynamic-state memory and its own
   hazard sets: images it reads, writes, samples from vertex shaders, and whether a render pass is
   left open at its end.
6. Shared state (textures, samplers, uploads; shaders) is guarded by a few locks with a fixed
   order. GPU writes a block makes are kept per thread and merged after recording.
7. After recording, one thread executes the blocks' command buffers into the primary in stream
   order. Between blocks it inserts only the barrier the hazard sets need (a full barrier for a
   target written and then sampled, a pass barrier for an attachment written again), and lets a
   block continue a render pass the previous block left open.
8. Uploads a block found missing are done before the first block that needs them, once per image
   version.
9. The cost per draw of each block is kept for the next frame's cut.

## How SpartanPort differs

- Streaming, not whole-submission: stage A decodes PM4 as it arrives, stage B resolves and records
  draw by draw (`vk_draw_pipe.h`), and the scheduler's recording thread turns the recorded
  closures into Vulkan commands. Vulkan command recording itself is already off stage B.
- Stage B's time is resolution, not command recording (City of Olympia, docs/parallel_gpu.md):
  bind 21%, inputs 20%, record 16%, begin 11%. Resolution goes through the buffer cache, texture
  cache, page tracking and barrier state, which have one owner (stage B) and no locks.
- Barriers are tracked globally by `runtime` (`IsBufferAccessed`, `FlushBarriers`) as draws are
  resolved, not per block.

## Requirements (EARS)

1. When a guest submission is complete and the toggle is on, the system shall split its direct
   draws into blocks at render target changes and resolve the blocks on N threads.
2. While a block is resolved, the system shall see the registers as they were at the block's
   first draw, and the GPU writes queued before the block's first draw.
3. If a block needs work that orders later draws (a dispatch, a guest memory write, a draw into a
   linear target, a query read by the CPU), the system shall cut there and do that work serially.
4. If two blocks touch the same image or buffer and one writes it, the system shall order them
   with the barrier serial recording would have recorded.
5. The system shall always produce the same commands, in the same order of effect, as serial
   recording; `SPARTAN_PIPE_VERIFY` compares them.

## Feasibility in SpartanPort

The gain is in parallel resolution, which needs the buffer cache, texture cache and barrier
tracking safe on several threads (locks or per-thread parts merged at the end). That is the area of
the past heap corruption and the largest change of the roadmap.

Phases, each behind its toggle and measured by live A/B in the City of Olympia:

- **P0: measure.** Time of stage B per block-sized run of draws, and how much of it touches shared
  caches (lock candidates) against block-local work. Without P0 the split is a guess.
  Measured on 2026-11-19 (City of Olympia, 1080p, temporary counters at the cache entry points,
  nested calls counted once, stage B only): the buffer and texture caches take ~13% of stage B's
  window: `ObtainBuffer` 7% (~560k calls/s), `ResolveReadbacks` called directly 4.5% (only
  ~210 calls/s, ~0.2 ms each: waits for GPU-to-RAM copies), texture views 0.8%, image and target
  lookups 0.6%. The caches are not what limits a split; the order-dependent state outside them
  is: barrier tracking in `runtime` (`IsBufferAccessed`, `FlushBarriers`), the recording stream,
  and the draw-to-draw memos. P1 has to give each block its own copy of that state and merge the
  barriers between blocks, as the observed renderer does with its hazard sets.
- **P1: whole-submission batching.** Stage B takes a complete submission and resolves it serially
  as blocks with saved registers and per-block hazard sets, still on one thread. Its output must
  match serial recording under `SPARTAN_PIPE_VERIFY`. No gain expected; it is the base for P2.
  Where the per-block state goes (code read on 2026-11-19): every image layout change passes
  `Runtime::Transit` (`vk_runtime.h`), and buffer hazards pass `Runtime::AccessBuffer` and
  `IsBufferAccessed` over one `barrier_tracker`. In block mode those three record, per block, the
  layout each image needs at its first use, its layout at the end, and the buffer ranges read and
  written, instead of changing the shared state. A stitch step after the blocks walks them in
  stream order, compares each block's needs with the state the blocks before left, emits the
  transitions and memory barriers between them, and then applies each block's end state. P1 is
  correct when this stitch, with one resolver, records the barriers serial recording records
  (a recording diff under `SPARTAN_PIPE_VERIFY`).
- **P2: two resolvers.** Blocks without shared writes on two threads, caches under a lock. The A/B
  decides whether the lock costs more than the second thread gives.
- **P3: more resolvers and the cost-based cut.**

Acceptance at every phase: no fault, `SPARTAN_PIPE_VERIFY` clean, gain over twice the noise in the
City of Olympia; otherwise the phase is reverted and recorded in docs/parallel_gpu.md.

## Decision of 2026-11-19: blocks, not a longer pipeline

Measured in the City of Olympia at 1080p (99-102 FPS, toggles 0): stage A busy ~85% (pipeline
selection 0.95-1.03 us/draw, constants 0.87-0.98, hand-off 0.26), stage B busy ~91% (3.5 us/draw:
bind 0.80, inputs 0.78, record 0.66, task 0.32, begin 0.24), the Vulkan recording thread busy
~67%. Readback waits were down to a few per minute (no longer worth a change).

A longer pipeline (each busy thread cut in two) was costed at +20-25% at best: stage B splits
cleanly only at its record tail (~20% of it); bind and inputs share the buffer and texture caches,
which a split would have to lock anyway. Felipe chose the blocks directly. Every step below is
behind a toggle, off by default; the game runs today's path until the gate, `SPARTAN_PIPE_VERIFY`,
the patches and a run of each supported title version pass with it on (the automated scenarios
only cover one version).

Implementation order inside P1 to P2:

1. **Ordered block streams in the scheduler.** A block records its commands into its own chunk
   list; the stitch splices the lists into the command stream in block order, with the barriers
   between them. Unit-tested on the chunk lists alone.
2. **Per-block barrier capture.** `Runtime::Transit`, `AccessBuffer` and `IsBufferAccessed`
   record a block's first-use needs and end state instead of changing the shared state; the
   stitch resolves them in order (P1 above).
3. **Per-resolver draw context.** The per-draw scratch state of `Rasterizer` (descriptor writes,
   push data, bound buffers and images, render target descriptors, draw inputs, memos) moves into
   a context object each resolver owns; stage B keeps one.
4. **Register snapshots.** Each resolver applies every packet's register blocks to its own copy
   (0.16 us/draw) and resolves only the draws of its blocks.
5. **Shared caches under locks.** Buffer cache, texture cache, page manager and the frame-wide
   hooks (upscaler, camera and object motion, scene targets) behind a fixed lock order; hooks with
   draw-order effects run in the stitch.
6. **Two resolvers (P2), then N with the cost-based cut (P3).**

### Design notes for P2 (code read on 2026-11-19)

- Vulkan commands stay recorded by the one recording thread: block streams are closure lists
  spliced in order into the same primary command buffer (`RecordStream`, step 1, done). That
  thread is ~67% busy today, so parallel resolution alone tops out near 1.5x; going further
  needs secondary command buffers per resolver (a later step).
- Image layouts live in `Image::backing->state` and `subresource_states`. In block mode,
  `Image::GetBarriers` works on a block-local shadow of each image it touches: the first use
  records what the block needs (layout, access, stage per subresource) and emits nothing; later
  uses in the block see the shadow. The stitch, serial and in block order, emits the barrier from
  the real state to each block's first needs before splicing the block, then writes the block's
  end shadow state back to the real image.
- Buffer hazards: each block keeps its own barrier tracker and pending memory barrier. The stitch
  emits one memory barrier before a block when an earlier block since the last flush wrote memory
  (conservative: one barrier per block boundary at most).
- Render passes: a block ends its render pass at its end, and the next block starts its own
  (continuing an open pass across blocks is a later optimization). Dynamic state and the
  draw-to-draw memos start empty in every block, so the first draw of a block sets everything.
- Order-dependent hooks in `DrawRecord` (frame start and display pass detection, upscaler,
  camera and object motion) stay serial: block mode is only used while the upscaler and motion
  vectors are off, at first; draws that start a frame or touch a display buffer cut the block.
- Guest memory written by queued GPU work (WriteData, DMA, ordered tasks) cuts the block, so a
  block never sees memory change under it.
- Stage A is ~85% busy and two thirds of that is pipeline selection and constant copies. Blocks
  only pay off once those move into the resolvers too (stage A then only decodes PM4 and keeps
  the registers, ~1.2 us/draw). Expected steps of gain: parallel resolvers with pipeline
  selection and constants inside them, up to the recording thread's limit (~1.5x); then
  secondary command buffers per resolver, up to stage A's decode (~3x).
- Per-resolver state without moving code: a resolver is a second `Rasterizer` object whose
  shared parts (caches, page manager, pipeline cache, draw pipe, constant ring, upscaler, motion,
  frame-level flags) are references to the one the renderer owns, while the per-draw scratch
  (descriptor writes, push data, bound buffers and images, target descriptors, draw inputs,
  memos) stays per object. Method bodies compile unchanged; function-local statics in the draw
  path have to be checked one by one (counters may race, state may not).

### P2 checklist (2026-11-19, after P1a and P1b landed)

P1a (one draw per block) and P1b (up to 64 held draws per block, recorded on the resolver, still
on stage B) work behind toggle 1 << 62 with the same image and no fault in the static scene and
the City of Olympia; P1b costs ~5% there (serial). Next, in order, each step tested serially
before the next:

1. Render pass and dynamic state per block stream: a block starts with no pass open and all
   dynamic state dirty and closes its pass at its end; a splice closes the owner's pass first
   and marks its dynamic state dirty after; an early splice hands the stream's state over.
2. Host copy completion counted, not stamped with a sequence number (blocks recorded in parallel
   issue copies out of order).
3. Barrier state per block in `Runtime` (tracker, pending image and memory barriers) and image
   layout shadows with first needs; the stitch emits the barriers from the real state to each
   block's first needs and writes the block's end state back; the rasterizer's reads of
   `backing->state.layout` (descriptor and attachment layouts) go through the shadow.
4. A worker thread with a second resolver: the worker takes the earlier block, stage B the later
   one, joined and spliced in order; caches under one lock around the resolve part of
   `DrawRecord`; blocks only while the upscaler and motion vectors are off, display draws cut.
   An early splice on the later block is a fatal error (not seen in the measured scenes).
5. Measure; narrow the lock where memo hits do not touch the caches.

### P2 state on 2026-11-19 (night)

Two resolvers record blocks in parallel with the same image and no fault (static scene, City of
Olympia, toggle 1 << 62). Blocks were short at first (12 to 21 draws): stage B closed a block
whenever the ring was empty for an instant (~8,000 times a second, against ~1,100 real cuts:
tasks ~700/s, dispatches ~60/s, kept-out draws ~300/s). Waiting up to 30 us for the next packet
(at once when stage A drains or waits for ring space) raised blocks to 47 draws and the share
recorded in parallel to ~35%. Still slower than serial (City of Olympia 67 FPS against 85-100):
both resolvers hold the one lock for most of `DrawRecord` (bind, inputs, begin), so they take turns,
and each block adds a stitch, a full barrier and a render pass break. The gain needs the lock
narrowed: the memo hits of bind and inputs (most draws) must run without it, which needs the
buffer and texture cache lookups safe against a concurrent writer.

Measured with temporary counters (City of Olympia, toggle 1 << 62, after vertex and index inputs
left the lock: `ObtainBuffer` takes it itself): stage B resolved its own blocks 52% of the window
and the worker 48%; lock waits (both threads) 12%; waiting for the worker 5%; stitch and splice 3%;
ordered tasks, which only stage B runs, 16%. Per draw the two threads together spend ~60% more CPU
than serial recording, and FPS stayed at 64-67. Next, by size: binding (textures and buffers)
out of the lock, which needs the texture cache safe per call (image view lists grow under
`FindView`); tasks off stage B where they do not touch guest memory a block reads; the full
barrier per block replaced by the union of the blocks' buffer hazards; and a render pass left
open across blocks when the next one starts with the same targets.

### P2 tuning on 2026-11-20 (City of Olympia, 1080p, toggle 1 << 62; serial 96-106 FPS)

- Spin lock instead of `std::recursive_mutex`: 64-67 to 70 FPS. Buffer binding out of the lock: 71.
- Two workers, stage B recording a block only when none is free: 81. Three workers: no better.
- Parallel resolvers copy their own small stream reads outside the lock instead of queueing them
  for the Vulkan recording thread, which WriteData and fences then waited for: tasks 18% to 0.7%
  of stage B; 84-89 FPS.
- Blocks of 32 draws: 88.1-89.3 (64: 84-89; 16: 85).
- Memory barrier between blocks only after a block wrote buffers: 90.7-91.1 (the unconditional
  barrier cost ~5%, measured by removing it).
- Tried and dropped: stage B only dispatching and splicing (blocks streamed to workers and
  spliced as they finish): 87-88 with two workers, 86-87 with three, 85-86 with four. The workers
  then wait on the resolve lock (texture binding, render targets, the upscaler's hooks), so more
  of them do not help.
- Fixed: the per-draw image flags (`binding`: bound, target, rebind, general layout) are shared by
  resolvers drawing at the same time, and one resolver's reset cleared another's. Each image now
  counts the draws holding its flags and clears them when the last one is done.

### P2 state on 2026-11-20 (afternoon; tests on the 1080p C27M, free camera off)

- Guest MSAA images counted as up to date (toggle 1 << 63, docs/parallel_gpu.md) also helps
  here: memoized texture sets now hit ~98% of the time, so fewer binds take the lock.
- Pending operations and frame capture polled once per parallel block instead of per draw:
  City of Olympia 95.8 and 98.7 FPS with toggle 1 << 62 against 98.7-99.2 serial in the same
  session: parity, at ~284k draws/s either way.
- Three workers: 95.6 and 97.6 (no better than two).
- The constant ring off with blocks (stage A no longer copies constants, the resolvers read them
  through ObtainBuffer): stage A waits 46% of the time, but stage B binding rises from 0.64 to
  1.30 us/draw and stage B is 95% busy: 88.1 and 88.9 FPS. Dropped.
- Where stage B waits now: the resolve lock at the start of each draw (~0.58 us/draw), held about
  10% of the window by the start of draws (render targets, upscaler hooks), 6% by render pass
  setup, 2% by ObtainBuffer, the rest by texture binding. Stage A is ~78% busy: past ~1.25x it
  becomes the limit, so a larger gain also needs pipeline selection and constants off stage A.
