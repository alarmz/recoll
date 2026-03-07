//! CLI 參數解析測試

use clap::Parser;
use rn_cli::{Cli, Command, ThrottleArg};

#[test]
fn parse_init_subcommand() {
    let cli = Cli::parse_from(["rn", "init", "/data"]);
    match cli.command {
        Command::Init { path, force } => {
            assert_eq!(path.to_str().unwrap(), "/data");
            assert!(!force);
        }
        _ => panic!("expected Init"),
    }
}

#[test]
fn parse_search_with_defaults() {
    let cli = Cli::parse_from(["rn", "search", "hello"]);
    match cli.command {
        Command::Search {
            query,
            limit,
            offset,
            json,
            no_snippet,
            ..
        } => {
            assert_eq!(query, "hello");
            assert_eq!(limit, 20);
            assert_eq!(offset, 0);
            assert!(!json);
            assert!(!no_snippet);
        }
        _ => panic!("expected Search"),
    }
}

#[test]
fn parse_search_with_all_options() {
    let cli = Cli::parse_from([
        "rn",
        "search",
        "--limit",
        "5",
        "--offset",
        "10",
        "--json",
        "--no-snippet",
        "--type",
        "pdf",
        "query text",
    ]);
    match cli.command {
        Command::Search {
            query,
            limit,
            offset,
            json,
            no_snippet,
            file_type,
        } => {
            assert_eq!(query, "query text");
            assert_eq!(limit, 5);
            assert_eq!(offset, 10);
            assert!(json);
            assert!(no_snippet);
            assert_eq!(file_type.as_deref(), Some("pdf"));
        }
        _ => panic!("expected Search"),
    }
}

#[test]
fn parse_index_with_flags() {
    let cli = Cli::parse_from([
        "rn",
        "index",
        "--full",
        "--dry-run",
        "--throttle",
        "gentle",
        "/path",
    ]);
    match cli.command {
        Command::Index {
            path,
            full,
            dry_run,
            throttle,
        } => {
            assert_eq!(path.to_str().unwrap(), "/path");
            assert!(full);
            assert!(dry_run);
            assert_eq!(throttle, ThrottleArg::Gentle);
        }
        _ => panic!("expected Index"),
    }
}

#[test]
fn parse_stats_json() {
    let cli = Cli::parse_from(["rn", "stats", "--json"]);
    match cli.command {
        Command::Stats { json } => assert!(json),
        _ => panic!("expected Stats"),
    }
}

#[test]
fn parse_doctor_flags() {
    let cli = Cli::parse_from(["rn", "doctor", "--fix", "--verbose"]);
    match cli.command {
        Command::Doctor { fix, verbose } => {
            assert!(fix);
            assert!(verbose);
        }
        _ => panic!("expected Doctor"),
    }
}
