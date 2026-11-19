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
