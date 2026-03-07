use crate::config::ServiceConfig;

/// 產生 sc.exe create 命令參數
pub fn install_args(config: &ServiceConfig) -> Vec<String> {
    vec![
        "create".to_string(),
        config.name.clone(),
        format!("binPath={}", config.binary_name),
        format!("DisplayName={}", config.display_name),
        "start=auto".to_string(),
    ]
}

/// 產生 sc.exe delete 命令參數
pub fn uninstall_args(config: &ServiceConfig) -> Vec<String> {
    vec!["delete".to_string(), config.name.clone()]
}

/// 產生 sc.exe start 命令參數
pub fn start_args(config: &ServiceConfig) -> Vec<String> {
    vec!["start".to_string(), config.name.clone()]
}

/// 產生 sc.exe stop 命令參數
pub fn stop_args(config: &ServiceConfig) -> Vec<String> {
    vec!["stop".to_string(), config.name.clone()]
}
