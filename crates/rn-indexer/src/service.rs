/// 服務狀態
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ServiceState {
    Idle,
    Running,
    Paused,
    Stopped,
}

impl ServiceState {
    pub fn can_transition_to(&self, target: ServiceState) -> bool {
        use ServiceState::*;
        matches!(
            (self, target),
            (Idle, Running)
                | (Running, Paused)
                | (Running, Stopped)
                | (Paused, Running)
                | (Paused, Stopped)
        )
    }
}
