/// I/O 節流模式
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IoThrottle {
    Off,
    Gentle,
    Aggressive,
}

impl IoThrottle {
    pub fn delay_ms(&self) -> u64 {
        match self {
            Self::Off => 0,
            Self::Gentle => 10,
            Self::Aggressive => 100,
        }
    }
}
