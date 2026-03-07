use rn_core::task::TaskPriority;
use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::path::PathBuf;

/// 優先任務
#[derive(Debug, Clone)]
pub struct PrioritizedTask {
    pub path: PathBuf,
    pub priority: TaskPriority,
    pub sequence: u64,
}

impl Eq for PrioritizedTask {}

impl PartialEq for PrioritizedTask {
    fn eq(&self, other: &Self) -> bool {
        self.priority == other.priority && self.sequence == other.sequence
    }
}

impl Ord for PrioritizedTask {
    fn cmp(&self, other: &Self) -> Ordering {
        // Higher priority (lower enum value) first, then FIFO (lower sequence first)
        other
            .priority
            .cmp(&self.priority)
            .then(other.sequence.cmp(&self.sequence))
    }
}

impl PartialOrd for PrioritizedTask {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

/// 優先佇列
pub struct TaskQueue {
    heap: BinaryHeap<PrioritizedTask>,
}

impl TaskQueue {
    pub fn new() -> Self {
        Self {
            heap: BinaryHeap::new(),
        }
    }

    pub fn push(&mut self, task: PrioritizedTask) {
        self.heap.push(task);
    }

    pub fn pop(&mut self) -> Option<PrioritizedTask> {
        self.heap.pop()
    }

    pub fn len(&self) -> usize {
        self.heap.len()
    }

    pub fn is_empty(&self) -> bool {
        self.heap.is_empty()
    }
}

impl Default for TaskQueue {
    fn default() -> Self {
        Self::new()
    }
}
