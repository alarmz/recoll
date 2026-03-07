use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum DocumentState {
    Discovered,
    Queued,
    Extracting,
    Extracted,
    Normalized,
    Indexed,
    Failed { error: String, retry_count: u8 },
    Stale,
    Deleted,
}

pub fn can_transition(from: &DocumentState, to: &DocumentState) -> bool {
    use DocumentState::*;
    matches!(
        (from, to),
        (Discovered, Queued)
            | (Queued, Extracting)
            | (Extracting, Extracted)
            | (Extracted, Normalized)
            | (Normalized, Indexed)
            | (Indexed, Stale)
            | (Stale, Queued)
            | (Failed { .. }, Queued)
            | (_, Failed { .. })
            | (Indexed, Deleted)
            | (Stale, Deleted)
    )
}
