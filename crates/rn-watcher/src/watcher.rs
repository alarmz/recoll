use anyhow::Result;
use crossbeam_channel::Receiver;
use notify::{RecommendedWatcher, RecursiveMode, Watcher};
use std::path::Path;

use crate::event_mapper::{map_event, MappedEvent};

/// 檔案系統監控器
pub struct FsWatcher {
    _watcher: RecommendedWatcher,
    rx: Receiver<MappedEvent>,
}

impl FsWatcher {
    /// 建立監控器並開始監控指定路徑
    pub fn new(path: &Path) -> Result<Self> {
        let (tx, rx) = crossbeam_channel::unbounded();

        let mut watcher =
            notify::recommended_watcher(move |res: notify::Result<notify::Event>| {
                if let Ok(event) = res {
                    let mapped = map_event(&event.kind, &event.paths);
                    for m in mapped {
                        let _ = tx.send(m);
                    }
                }
            })?;

        watcher.watch(path, RecursiveMode::Recursive)?;

        Ok(Self {
            _watcher: watcher,
            rx,
        })
    }

    /// 接收下一個事件（無限阻塞）
    pub fn recv(&self) -> Option<MappedEvent> {
        self.rx.recv().ok()
    }

    /// 接收下一個事件（阻塞，帶 timeout）
    pub fn recv_timeout(&self, timeout: std::time::Duration) -> Option<MappedEvent> {
        self.rx.recv_timeout(timeout).ok()
    }
}
