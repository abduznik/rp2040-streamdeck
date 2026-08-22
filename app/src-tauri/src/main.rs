#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};
use std::io::Read;
use std::sync::Mutex;
use tauri::Manager;
use tauri::State;

const SOF: u8 = 0xAA;
const CMD_GET_MAPPING: u8 = 0x01;
const CMD_SET_BUTTON: u8 = 0x02;
const CMD_SAVE_FLASH: u8 = 0x03;
const CMD_PING: u8 = 0x04;
const REPLY_MAPPING: u8 = 0x81;
const REPLY_BUTTON_ACK: u8 = 0x82;
const REPLY_SAVE_ACK: u8 = 0x83;
const REPLY_PONG: u8 = 0x84;
const NUM_BUTTONS: usize = 6;
const ACTION_SIZE: usize = 283;

#[derive(Serialize, Deserialize, Clone, Debug, Default)]
struct ButtonAction {
    #[serde(rename = "type", default)]
    action_type: u8,
    #[serde(default)] modifier: u8,
    #[serde(default)] keycode: u8,
    #[serde(default)] consumer_code: u16,
    #[serde(default)] text: String,
    #[serde(default)] launcher_os: u8,
    #[serde(default)] label: String,
    #[serde(default)] macro_steps: Vec<MacroStep>,
    #[serde(default)] macro_count: u8,
}

#[derive(Serialize, Deserialize, Clone, Debug, Default)]
struct MacroStep { #[serde(default)] modifier: u8, #[serde(default)] keycode: u8 }

struct AppState {
    port: Mutex<Option<Box<dyn serialport::SerialPort>>>,
    port_name: Mutex<String>,
    read_buf: Mutex<Vec<u8>>,
}

fn drain_port(port: &mut Box<dyn serialport::SerialPort>) {
    port.set_timeout(std::time::Duration::from_millis(20)).ok();
    let mut trash = [0u8; 4096];
    loop { match port.read(&mut trash) { Ok(0) | Err(_) => break, Ok(_) => continue } }
}

fn send_command(port: &mut Box<dyn serialport::SerialPort>, cmd: u8, payload: &[u8]) -> Result<(), String> {
    let len = payload.len();
    let len_lo = (len & 0xFF) as u8;
    let len_hi = ((len >> 8) & 0xFF) as u8;
    let mut cs = cmd.wrapping_add(len_lo).wrapping_add(len_hi);
    for &b in payload { cs = cs.wrapping_add(b); }
    let mut pkt = vec![SOF, cmd, len_lo, len_hi];
    pkt.extend_from_slice(payload);
    pkt.push(cs);
    port.write_all(&pkt).map_err(|e| e.to_string())?;
    port.flush().map_err(|e| e.to_string())?;
    Ok(())
}

fn read_exact_packet(read_buf: &mut Vec<u8>, port: &mut Box<dyn serialport::SerialPort>, expected_cmd: u8) -> Result<Vec<u8>, String> {
    let deadline = std::time::Instant::now() + std::time::Duration::from_secs(5);
    port.set_timeout(std::time::Duration::from_millis(200)).ok();

    loop {
        // Try to parse from existing buffer first
        while let Some(sof_idx) = read_buf.iter().position(|&b| b == SOF) {
            if sof_idx > 0 { read_buf.drain(..sof_idx); }
            if read_buf.len() < 4 { break; }
            let cmd = read_buf[1];
            let len = read_buf[2] as usize | ((read_buf[3] as usize) << 8);
            if len > 4096 { read_buf.drain(..1); continue; } // bad len, skip SOF
            let pkt_len = 4 + len + 1;
            if read_buf.len() < pkt_len { break; } // need more data
            let payload = read_buf[4..4+len].to_vec();
            read_buf.drain(..pkt_len);
            if cmd == expected_cmd { return Ok(payload); }
            // Wrong cmd, skip and keep searching
        }

        if std::time::Instant::now() > deadline {
            return Err(format!("Timeout waiting for reply 0x{:02X}", expected_cmd));
        }

        // Read more bytes from port
        let mut chunk = [0u8; 1024];
        match port.read(&mut chunk) {
            Ok(n) if n > 0 => read_buf.extend_from_slice(&chunk[..n]),
            Ok(_) => {}
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
            Err(e) => return Err(e.to_string()),
        }
    }
}

fn launch_app(app_name: &str) {
    let name = app_name.trim();
    if name.is_empty() { return; }
    #[cfg(target_os = "macos")]
    { let _ = std::process::Command::new("open").args(["-a", name]).spawn(); }
    #[cfg(target_os = "windows")]
    { let _ = std::process::Command::new("cmd").args(["/c", "start", "", name]).spawn(); }
    #[cfg(target_os = "linux")]
    { let _ = std::process::Command::new("xdg-open").arg(name).spawn(); }
}

// --- Tauri commands ---

#[tauri::command]
fn list_ports() -> Result<Vec<String>, String> {
    Ok(serialport::available_ports().map_err(|e| e.to_string())?.iter().map(|p| p.port_name.clone()).collect())
}

#[tauri::command]
fn connect_port(port_name: String, state: State<AppState>) -> Result<String, String> {
    let mut port = serialport::new(&port_name, 115200).open().map_err(|e| format!("Failed to open {}: {}", port_name, e))?;
    drain_port(&mut port);
    port.set_timeout(std::time::Duration::from_millis(200)).ok();
    send_command(&mut port, CMD_PING, &[])?;
    let mut buf = state.read_buf.lock().unwrap();
    buf.clear();
    let reply = read_exact_packet(&mut buf, &mut port, REPLY_PONG)?;
    let fw = reply.first().copied().unwrap_or(0);
    drop(buf);
    *state.port.lock().unwrap() = Some(port);
    *state.port_name.lock().unwrap() = port_name.clone();
    Ok(format!("Connected to {} (firmware v{})", port_name, fw))
}

#[tauri::command]
fn disconnect_port(state: State<AppState>) -> Result<(), String> {
    *state.port.lock().unwrap() = None;
    *state.port_name.lock().unwrap() = String::new();
    state.read_buf.lock().unwrap().clear();
    Ok(())
}

#[tauri::command]
fn get_port(state: State<AppState>) -> Result<String, String> {
    Ok(state.port_name.lock().unwrap().clone())
}

#[tauri::command]
fn get_mapping(state: State<AppState>) -> Result<Vec<ButtonAction>, String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;
    drain_port(port);
    send_command(port, CMD_GET_MAPPING, &[])?;
    let mut buf = state.read_buf.lock().unwrap();
    let payload = read_exact_packet(&mut buf, port, REPLY_MAPPING)?;
    drop(buf);

    let mut buttons = Vec::new();
    for i in 0..NUM_BUTTONS {
        let off = 8 + i * ACTION_SIZE; // skip magic(4) + version(4)
        if off + ACTION_SIZE > payload.len() { break; }
        let t = payload[off];
        let d = &payload[off+1..off+1+258];
        let lb = &payload[off+1+258..off+1+258+24];
        let label = String::from_utf8_lossy(lb).trim_end_matches('\0').to_string();
        let mut a = ButtonAction { action_type: t, label, ..Default::default() };
        match t {
            1 => { a.modifier = d[0]; a.keycode = d[1]; }
            2 => { a.consumer_code = d[0] as u16 | ((d[1] as u16) << 8); }
            3 => {
                a.macro_count = d[32];
                for s in 0..16 { a.macro_steps.push(MacroStep { modifier: d[s*2], keycode: d[s*2+1] }); }
            }
            4 | 5 => { let l = d[256] as usize | ((d[257] as usize) << 8); a.text = String::from_utf8_lossy(&d[..l.min(256)]).to_string(); }
            6 => { a.launcher_os = d[0]; a.text = String::from_utf8_lossy(&d[1..]).trim_end_matches('\0').to_string(); }
            _ => {}
        }
        buttons.push(a);
    }
    Ok(buttons)
}

#[tauri::command]
fn set_button(idx: usize, action: ButtonAction, state: State<AppState>) -> Result<(), String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;
    if idx >= NUM_BUTTONS { return Err("Button index out of range".into()); }
    let mut d = vec![0u8; ACTION_SIZE];
    d[0] = action.action_type;
    match action.action_type {
        1 => { d[1] = action.modifier; d[2] = action.keycode; }
        2 => { d[1] = (action.consumer_code & 0xFF) as u8; d[2] = ((action.consumer_code >> 8) & 0xFF) as u8; }
        3 => {
            for (i, s) in action.macro_steps.iter().enumerate().take(16) { d[1+i*2] = s.modifier; d[1+i*2+1] = s.keycode; }
            d[1+32] = action.macro_count;
        }
        4 | 5 => { let b = action.text.as_bytes(); let l = b.len().min(255); d[1..1+l].copy_from_slice(&b[..l]); d[1+256] = l as u8; }
        6 => { d[1] = action.launcher_os; let b = action.text.as_bytes(); let l = b.len().min(255); d[2..2+l].copy_from_slice(&b[..l]); }
        _ => {}
    }
    let mut payload = vec![idx as u8]; payload.extend_from_slice(&d);
    drain_port(port);
    send_command(port, CMD_SET_BUTTON, &payload)?;
    let mut buf = state.read_buf.lock().unwrap();
    let reply = read_exact_packet(&mut buf, port, REPLY_BUTTON_ACK)?;
    if reply.first() != Some(&(idx as u8)) { return Err("Device rejected".into()); }
    Ok(())
}

#[tauri::command]
fn save_flash(state: State<AppState>) -> Result<bool, String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;
    drain_port(port);
    send_command(port, CMD_SAVE_FLASH, &[])?;
    let mut buf = state.read_buf.lock().unwrap();
    let reply = read_exact_packet(&mut buf, port, REPLY_SAVE_ACK)?;
    Ok(reply.first() == Some(&1u8))
}

#[tauri::command]
fn poll_button_press(state: State<AppState>) -> Result<Option<usize>, String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;
    port.set_timeout(std::time::Duration::from_millis(50)).ok();
    let mut read_buf = state.read_buf.lock().unwrap();
    let mut chunk = [0u8; 256];
    if let Ok(n) = port.read(&mut chunk) { read_buf.extend_from_slice(&chunk[..n]); }
    while let Some(pos) = read_buf.iter().position(|&b| b == 0xBE) {
        if pos + 1 < read_buf.len() {
            let idx = read_buf[pos + 1] as usize;
            read_buf.drain(..=pos + 1);
            if idx < NUM_BUTTONS { return Ok(Some(idx)); }
        } else { break; }
    }
    Ok(None)
}

#[tauri::command]
fn launch_app_by_name(app_name: String) -> Result<(), String> {
    if app_name.trim().is_empty() { return Err("No app name".into()); }
    launch_app(&app_name);
    Ok(())
}

#[tauri::command]
fn list_installed_apps() -> Result<Vec<String>, String> {
    let mut apps = Vec::new();
    #[cfg(target_os = "macos")]
    if let Ok(o) = std::process::Command::new("mdfind").args(["kMDItemKind == 'Application'"]).output() {
        for line in String::from_utf8_lossy(&o.stdout).lines() {
            if let Some(n) = std::path::Path::new(line).file_stem().and_then(|s| s.to_str()) { apps.push(n.to_string()); }
        }
    }
    #[cfg(target_os = "linux")]
    if let Ok(o) = std::process::Command::new("ls").arg("/usr/share/applications/").output() {
        for line in String::from_utf8_lossy(&o.stdout).lines() {
            if let Some(n) = line.strip_suffix(".desktop") { apps.push(n.replace('-', " ")); }
        }
    }
    apps.sort(); apps.dedup(); Ok(apps)
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(AppState { port: Mutex::new(None), port_name: Mutex::new(String::new()), read_buf: Mutex::new(Vec::new()) })
        .setup(|app| {
            // Prevent window from being destroyed on close — hide to tray instead
            {
                use tauri::Manager;
                if let Some(window) = app.get_webview_window("main") {
                    let win = window.clone();
                    window.on_window_event(move |event| {
                        if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                            win.hide().unwrap();
                            api.prevent_close();
                        }
                    });
                }
            }

            // Set up tray icon
            {
                use tauri::menu::{Menu, MenuItem};
                use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};

                let show_item = MenuItem::with_id(app, "show", "Show Window", true, None::<&str>)?;
                let quit_item = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
                let menu = Menu::with_items(app, &[&show_item, &quit_item])?;

                let _tray = TrayIconBuilder::with_id("streamdeck-tray")
                    .icon(app.default_window_icon().unwrap().clone())
                    .menu(&menu)
                    .show_menu_on_left_click(false)
                    .on_menu_event(move |app, event| {
                        match event.id.as_ref() {
                            "show" => {
                                if let Some(w) = app.get_webview_window("main") {
                                    w.show().unwrap();
                                    w.set_focus().unwrap();
                                }
                            }
                            "quit" => {
                                app.exit(0);
                            }
                            _ => {}
                        }
                    })
                    .on_tray_icon_event(|tray, event| {
                        if let TrayIconEvent::Click {
                            button: MouseButton::Left,
                            button_state: MouseButtonState::Up,
                            ..
                        } = event
                        {
                            let app = tray.app_handle();
                            if let Some(w) = app.get_webview_window("main") {
                                w.show().unwrap();
                                w.set_focus().unwrap();
                            }
                        }
                    })
                    .build(app)?;
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![list_ports, connect_port, disconnect_port, get_port, get_mapping, set_button, save_flash, poll_button_press, launch_app_by_name, list_installed_apps])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
