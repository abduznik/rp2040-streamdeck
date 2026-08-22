#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn launch_app(app_name: &str) {
    let app_name = app_name.trim();
    if app_name.is_empty() { return; }
    #[cfg(target_os = "macos")]
    { let _ = std::process::Command::new("open").args(["-a", app_name]).spawn(); }
    #[cfg(target_os = "windows")]
    { let _ = std::process::Command::new("cmd").args(["/c", "start", "", app_name]).spawn(); }
    #[cfg(target_os = "linux")]
    { let _ = std::process::Command::new("xdg-open").arg(app_name).spawn(); }
}

#[tauri::command]
fn launch_app_by_name(app_name: String) -> Result<(), String> {
    let name = app_name.trim().to_string();
    if name.is_empty() { return Err("No app name provided".into()); }
    launch_app(&name);
    Ok(())
}

#[tauri::command]
fn list_installed_apps() -> Result<Vec<String>, String> {
    let mut apps = Vec::new();
    #[cfg(target_os = "macos")]
    {
        if let Ok(output) = std::process::Command::new("mdfind").args(["kMDItemKind == 'Application'"]).output() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            for line in stdout.lines() {
                if let Some(name) = std::path::Path::new(line).file_stem().and_then(|s| s.to_str()) {
                    apps.push(name.to_string());
                }
            }
        }
    }
    #[cfg(target_os = "linux")]
    {
        if let Ok(output) = std::process::Command::new("ls").arg("/usr/share/applications/").output() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            for line in stdout.lines() {
                if let Some(name) = line.strip_suffix(".desktop") {
                    apps.push(name.replace('-', " ").to_string());
                }
            }
        }
    }
    apps.sort(); apps.dedup();
    Ok(apps)
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .invoke_handler(tauri::generate_handler![
            launch_app_by_name,
            list_installed_apps,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
