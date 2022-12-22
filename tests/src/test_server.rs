use signal_child::Signalable;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::process::{Child, Command, Stdio};

pub struct TestServer {
    server: Option<Child>,
    port: u16,
}

impl TestServer {
    pub fn start() -> Self {
        if let Ok(port) = std::env::var("SERVER_PORT_OVERRIDE") {
            return Self {
                server: None,
                port: port.parse().expect("can't parse SERVER_PORT_OVERRIDE"),
            };
        }

        let mut server = Command::new(env!("SERVER_PATH"))
            .arg("0")
            .env("TWIIIIITER_DATABASE_FILE", ":memory:")
            .stdout(Stdio::piped())
            .stdin(Stdio::null())
            .spawn()
            .expect("can't start server");

        let stdout = BufReader::new(server.stdout.take().unwrap());

        let port = stdout
            .lines()
            .map(|line| line.unwrap())
            .filter_map(|line| {
                line.strip_prefix("[INFO] Listening on *:")
                    .map(|port| port.parse::<u16>().expect("invalid port"))
            })
            .take(1)
            .next()
            .expect("server exited without providing its port");

        println!("Started server of PID {} on port {port}", server.id());
        std::io::stdout().flush().unwrap();

        Self {
            server: Some(server),
            port,
        }
    }

    pub fn connect(&self) -> std::io::Result<TcpStream> {
        TcpStream::connect(("localhost", self.port))
    }
}

impl Drop for TestServer {
    fn drop(&mut self) {
        if let Some(server) = &mut self.server {
            server.interrupt().unwrap();
            server.wait().unwrap();
        }
    }
}
