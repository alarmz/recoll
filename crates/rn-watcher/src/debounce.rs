use rn_core::task::OperationType;
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;
use std::time::{Duration, Instant};

/// 去抖動事件
#[derive(Debug, Clone)]
pub struct DebouncedEvent {
    pub path: PathBuf,
    pub operation: OperationType,
}

/// 事件去抖動器
pub struct Debouncer {
    window: Duration,
    pending: HashMap<PathBuf, PendingEvent>,
    immediate_ops: HashSet<OperationType>,
}

struct PendingEvent {
    operation: OperationType,
    first_seen: Instant,
    immediate: bool,
}

impl Debouncer {
    pub fn new(window: Duration) -> Self {
        let mut immediate_ops = HashSet::new();
        immediate_ops.insert(OperationType::Delete);
        Self {
            window,
            pending: HashMap::new(),
            immediate_ops,
        }
    }

    /// 設定哪些 OperationType 需要立即 flush
    pub fn with_immediate_ops(mut self, ops: &[OperationType]) -> Self {
        self.immediate_ops = ops.iter().copied().collect();
        self
    }

    /// 記錄一個事件
    pub fn record(&mut self, path: PathBuf, operation: OperationType) {
        let immediate = self.immediate_ops.contains(&operation);
        self.pending
            .entry(path)
            .and_modify(|e| {
                e.operation = operation;
                if immediate {
                    e.immediate = true;
                }
            })
            .or_insert(PendingEvent {
                operation,
                first_seen: Instant::now(),
                immediate,
            });
    }

    /// 回傳目前 pending 的路徑數量
    pub fn pending_count(&self) -> usize {
        self.pending.len()
    }

    /// 取出所有已超過窗口或標記為 immediate 的事件
    pub fn drain_ready(&mut self) -> Vec<DebouncedEvent> {
        let now = Instant::now();
        let window = self.window;
        let mut events = Vec::new();
        self.pending.retain(|path, e| {
            if e.immediate || now.duration_since(e.first_seen) >= window {
                events.push(DebouncedEvent {
                    path: path.clone(),
                    operation: e.operation,
                });
                false
            } else {
                true
            }
        });
        events
    }
}
