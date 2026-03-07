use std::time::{Duration, Instant};

/// 提交策略
#[derive(Debug, Clone)]
pub enum CommitPolicy {
    ByCount(usize),
    ByTime(Duration),
    Hybrid { count: usize, interval: Duration },
}

/// 提交追蹤器
pub struct CommitTracker {
    policy: CommitPolicy,
    count: usize,
    last_commit: Instant,
}

impl CommitTracker {
    pub fn new(policy: CommitPolicy) -> Self {
        Self {
            policy,
            count: 0,
            last_commit: Instant::now(),
        }
    }

    pub fn record_document(&mut self) {
        self.count += 1;
    }

    pub fn should_commit(&self) -> bool {
        match &self.policy {
            CommitPolicy::ByCount(n) => self.count >= *n,
            CommitPolicy::ByTime(d) => self.last_commit.elapsed() >= *d,
            CommitPolicy::Hybrid { count, interval } => {
                self.count >= *count || self.last_commit.elapsed() >= *interval
            }
        }
    }

    pub fn reset(&mut self) {
        self.count = 0;
        self.last_commit = Instant::now();
    }
}
