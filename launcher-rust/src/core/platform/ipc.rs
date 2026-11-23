//! The local connection to another program (the Discord client): a named pipe on Windows, a Unix
//! socket elsewhere. `connect` hands back the same `Transport` on both.

use std::io;
use std::path::Path;
use std::time::Duration;

/// A connection to a local peer.
pub trait Transport: Send {
    /// Writes all of `data`, or fails.
    fn write(&mut self, data: &[u8]) -> io::Result<()>;
    /// At most `size` bytes, waiting up to `timeout` (None: as long as it takes) for some. A read
    /// that finds nothing in time fails with `TimedOut` or `WouldBlock`; once the connection has
    /// ended a read returns nothing.
    fn read(&mut self, size: usize, timeout: Option<Duration>) -> io::Result<Vec<u8>>;
    /// Lets the connection go.
    fn close(&mut self);
}

/// Connects to the pipe (Windows) or socket (elsewhere) at `path`.
pub fn connect(path: &Path) -> io::Result<Box<dyn Transport>> {
    imp::connect(path)
}

#[cfg(windows)]
mod imp {
    use super::Transport;
    use std::fs;
    use std::io;
    use std::os::windows::io::AsRawHandle;
    use std::path::Path;
    use std::thread;
    use std::time::{Duration, Instant};
    use windows_sys::Win32::Foundation::HANDLE;
    use windows_sys::Win32::System::Pipes::{
        PeekNamedPipe, SetNamedPipeHandleState, NAMED_PIPE_MODE, PIPE_NOWAIT, PIPE_READMODE_BYTE,
    };

    /// A named pipe (Python `PipeTransport`), in non-blocking mode: a read polls the pipe every
    /// 10 ms until something arrives or its time is up, and a write the pipe cannot take whole
    /// fails at once, so a silent or stalled peer holds up neither the thread nor `close`.
    struct PipeTransport {
        pipe: Option<fs::File>,
    }

    pub fn connect(path: &Path) -> io::Result<Box<dyn Transport>> {
        let pipe = fs::OpenOptions::new().read(true).write(true).open(path)?;
        set_pipe_nowait(&pipe)?;
        Ok(Box::new(PipeTransport { pipe: Some(pipe) }))
    }

    impl PipeTransport {
        fn pipe(&mut self) -> io::Result<&mut fs::File> {
            self.pipe
                .as_mut()
                .ok_or_else(|| io::ErrorKind::NotConnected.into())
        }
    }

    impl Transport for PipeTransport {
        fn write(&mut self, data: &[u8]) -> io::Result<()> {
            use std::io::Write;
            if self.pipe()?.write(data)? != data.len() {
                return Err(io::Error::new(
                    io::ErrorKind::WriteZero,
                    "Discord pipe could not accept the frame",
                ));
            }
            Ok(())
        }

        fn read(&mut self, size: usize, timeout: Option<Duration>) -> io::Result<Vec<u8>> {
            use std::io::Read;
            let deadline = timeout.map(|timeout| Instant::now() + timeout);
            let pipe = self.pipe()?;
            loop {
                let available = pipe_bytes_available(pipe)? as usize;
                if available > 0 {
                    let mut data = vec![0; size.min(available)];
                    let read = pipe.read(&mut data)?;
                    data.truncate(read);
                    return Ok(data);
                }
                if deadline.is_some_and(|deadline| Instant::now() >= deadline) {
                    return Err(io::ErrorKind::TimedOut.into());
                }
                thread::sleep(Duration::from_millis(10));
            }
        }

        fn close(&mut self) {
            self.pipe = None;
        }
    }

    /// Puts the client end of a named pipe in non-blocking byte mode (`PIPE_NOWAIT`), as the Python
    /// Discord presence does: a read or a write then returns at once with what the pipe holds or can
    /// take, so a silent or stalled server never holds up the caller.
    fn set_pipe_nowait(pipe: &fs::File) -> io::Result<()> {
        let mode: NAMED_PIPE_MODE = PIPE_NOWAIT | PIPE_READMODE_BYTE;
        // SAFETY: the handle is the live pipe handle owned by `pipe`, which outlives the call; `mode`
        // is a valid value to read; the collection count and timeout are not changed (null).
        let done = unsafe {
            SetNamedPipeHandleState(
                pipe.as_raw_handle() as HANDLE,
                &mode,
                std::ptr::null(),
                std::ptr::null(),
            )
        };
        if done == 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(())
    }

    /// The bytes waiting to be read from a named pipe, without reading them (`PeekNamedPipe`); an
    /// error once the other end is gone.
    fn pipe_bytes_available(pipe: &fs::File) -> io::Result<u32> {
        let mut available = 0u32;
        // SAFETY: the handle is the live pipe handle owned by `pipe`, which outlives the call; there
        // is no buffer to copy into (null, 0); `available` is a valid u32 to write and the other two
        // out pointers are null.
        let done = unsafe {
            PeekNamedPipe(
                pipe.as_raw_handle() as HANDLE,
                std::ptr::null_mut(),
                0,
                std::ptr::null_mut(),
                &mut available,
                std::ptr::null_mut(),
            )
        };
        if done == 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(available)
    }
}

#[cfg(not(windows))]
mod imp {
    use super::Transport;
    use std::io;
    use std::os::unix::net::UnixStream;
    use std::path::Path;
    use std::time::Duration;

    /// How long a write waits for the peer: the runner's Discord POLL (Python `POLL_SECONDS`),
    /// repeated here because core never names the runner.
    const WRITE_TIMEOUT: Duration = Duration::from_millis(100);

    /// `EINVAL` in `<sys/errno.h>` on macOS.
    #[cfg(target_os = "macos")]
    const EINVAL: i32 = 22;

    /// A Unix socket (Python `SocketTransport`); a write gives up after WRITE_TIMEOUT.
    struct SocketTransport {
        stream: Option<UnixStream>,
    }

    pub fn connect(path: &Path) -> io::Result<Box<dyn Transport>> {
        let stream = UnixStream::connect(path)?;
        stream.set_write_timeout(Some(WRITE_TIMEOUT))?;
        Ok(Box::new(SocketTransport {
            stream: Some(stream),
        }))
    }

    /// The server side of an accepted connection, for the tests.
    #[cfg(test)]
    pub fn from_stream(stream: UnixStream) -> Box<dyn Transport> {
        Box::new(SocketTransport {
            stream: Some(stream),
        })
    }

    impl SocketTransport {
        fn stream(&mut self) -> io::Result<&mut UnixStream> {
            self.stream
                .as_mut()
                .ok_or_else(|| io::ErrorKind::NotConnected.into())
        }
    }

    impl Transport for SocketTransport {
        fn write(&mut self, data: &[u8]) -> io::Result<()> {
            use std::io::Write;
            self.stream()?.write_all(data)
        }

        fn read(&mut self, size: usize, timeout: Option<Duration>) -> io::Result<Vec<u8>> {
            use std::io::Read;
            let stream = self.stream()?;
            match stream.set_read_timeout(timeout) {
                // macOS refuses SO_RCVTIMEO once the peer has closed; the read cannot block then,
                // it returns what the peer wrote and then the end.
                #[cfg(target_os = "macos")]
                Err(error) if error.raw_os_error() == Some(EINVAL) => {}
                result => result?,
            }
            let mut data = vec![0; size];
            let read = stream.read(&mut data)?;
            data.truncate(read);
            Ok(data)
        }

        fn close(&mut self) {
            self.stream = None;
        }
    }
}

/// A transport over an accepted socket, for the tests of the presence.
#[cfg(all(test, not(windows)))]
pub fn from_stream(stream: std::os::unix::net::UnixStream) -> Box<dyn Transport> {
    imp::from_stream(stream)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn connecting_to_a_missing_path_is_not_found() {
        let dir = tempfile::tempdir().unwrap();
        let error = connect(&dir.path().join("discord-ipc-0")).err().unwrap();
        assert_eq!(error.kind(), io::ErrorKind::NotFound);
    }

    /// What the peer wrote before closing is still read, then the end; macOS refuses a read
    /// timeout on a socket whose peer has closed.
    #[cfg(not(windows))]
    #[test]
    fn reads_what_the_peer_wrote_before_closing() {
        use std::io::Write;
        let (ours, mut theirs) = std::os::unix::net::UnixStream::pair().unwrap();
        theirs.write_all(b"last frame").unwrap();
        drop(theirs);
        let mut transport = from_stream(ours);
        let timeout = Some(Duration::from_secs(1));
        assert_eq!(transport.read(64, timeout).unwrap(), b"last frame");
        assert_eq!(transport.read(64, timeout).unwrap(), b"");
    }
}
