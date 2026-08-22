#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};
use std::sync::Mutex;
use tauri::State;

// --- Protocol constants (must match firmware) ---
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
const ACTION_SIZE: usize = 283; // 1 + 258 + 24
const ACTION_LAUNCHER: u8 = 6;
const LAUNCHER_MAC: u8 = 0;
const LAUNCHER_WINDOWS: u8 = 1;
const LAUNCHER_LINUX: u8 = 2;

// --- App state ---
struct AppState {
    port: Mutex<Option<Box<dyn serialport::SerialPort>>>,
    port_name: Mutex<String>,
}

// --- Serial helpers ---

fn calc_checksum(cmd: u8, payload: &[u8]) -> u8 {
    let mut cs = cmd.wrapping_add(payload.len() as u8);
    // We need len_lo and len_hi separately
    let len_lo = (payload.len() & 0xFF) as u8;
    let len_hi = ((payload.len() >> 8) & 0xFF) as u8;
    cs = cmd.wrapping_add(len_lo).wrapping_add(len_hi);
    for &b in payload {
        cs = cs.wrapping_add(b);
    }
    cs
}

fn send_command(port: &mut Box<dyn serialport::SerialPort>, cmd: u8, payload: &[u8]) -> Result<(), String> {
    let len = payload.len();
    let len_lo = (len & 0xFF) as u8;
    let len_hi = ((len >> 8) & 0xFF) as u8;

    let mut cs = cmd.wrapping_add(len_lo).wrapping_add(len_hi);
    for &b in payload {
        cs = cs.wrapping_add(b);
    }

    let mut packet = vec![SOF, cmd, len_lo, len_hi];
    packet.extend_from_slice(payload);
    packet.push(cs);

    port.write_all(&packet).map_err(|e| e.to_string())?;
    port.flush().map_err(|e| e.to_string())?;
    Ok(())
}

fn read_reply(port: &mut Box<dyn serialport::SerialPort>, expected_cmd: u8) -> Result<Vec<u8>, String> {
    port.set_timeout(std::time::Duration::from_secs(3)).map_err(|e| e.to_string())?;
    let mut buf = [0u8; 4096];
    let mut total = 0usize;

    // Read until we get a complete packet
    loop {
        let n = port.read(&mut buf[total..]).map_err(|e| e.to_string())?;
        total += n;

        // Look for SOF
        if total < 4 { continue; }
        let sof_idx = buf[..total].iter().position(|&b| b == SOF).unwrap_or(0);
        if sof_idx > 0 {
            buf.copy_within(sof_idx..total, 0);
            total -= sof_idx;
        }
        if total < 4 { continue; }

        let cmd = buf[1];
        let len = buf[2] as usize | ((buf[3] as usize) << 8);
        let pkt_len = 4 + len + 1;
        if total < pkt_len { continue; }

        // Check if this is the reply we want (or a notification 0xBE)
        if cmd == 0xBE {
            // Button notification — skip for now, frontend will handle
            buf.copy_within(pkt_len..total, 0);
            total -= pkt_len;
            continue;
        }

        if cmd == expected_cmd {
            let payload = buf[4..4+len].to_vec();
            return Ok(payload);
        }

        // Wrong reply, skip
        buf.copy_within(pkt_len..total, 0);
        total -= pkt_len;
    }
}

// --- Tauri commands ---

#[tauri::command]
fn list_ports() -> Result<Vec<String>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    Ok(ports.iter().map(|p| p.port_name.clone()).collect())
}

#[tauri::command]
fn connect_port(port_name: String, state: State<AppState>) -> Result<String, String> {
    let port = serialport::new(&port_name, 115200)
        .open()
        .map_err(|e| format!("Failed to open {}: {}", port_name, e))?;

    let mut port = port;
    port.set_timeout(std::time::Duration::from_millis(100)).ok();

    // Ping to verify device is alive
    send_command(&mut port, CMD_PING, &[])?;
    let reply = read_reply(&mut port, REPLY_PONG)?;
    let fw_version = reply.first().copied().unwrap_or(0);

    *state.port.lock().unwrap() = Some(port);
    *state.port_name.lock().unwrap() = port_name.clone();

    Ok(format!("Connected to {} (firmware v{})", port_name, fw_version))
}

#[tauri::command]
fn disconnect_port(state: State<AppState>) -> Result<(), String> {
    *state.port.lock().unwrap() = None;
    *state.port_name.lock().unwrap() = String::new();
    Ok(())
}

#[tauri::command]
fn get_port(state: State<AppState>) -> Result<String, String> {
    Ok(state.port_name.lock().unwrap().clone())
}

#[derive(Serialize, Deserialize)]
struct ButtonAction {
    #[serde(rename = "type")]
    action_type: u8,
    #[serde(default)]
    modifier: u8,
    #[serde(default)]
    keycode: u8,
    #[serde(default)]
    consumer_code: u16,
    #[serde(default)]
    text: String,
    #[serde(default)]
    launcher_os: u8,
    #[serde(default)]
    label: String,
    #[serde(default)]
    macro_steps: Vec<MacroStep>,
    #[serde(default)]
    macro_count: u8,
}

#[derive(Serialize, Deserialize)]
struct MacroStep {
    modifier: u8,
    keycode: u8,
}

#[tauri::command]
fn get_mapping(state: State<AppState>) -> Result<Vec<ButtonAction>, String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;

    send_command(port, CMD_GET_MAPPING, &[])?;
    let payload = read_reply(port, REPLY_MAPPING)?;

    let mut buttons = Vec::new();
    for i in 0..NUM_BUTTONS {
        let offset = i * ACTION_SIZE;
        if offset + ACTION_SIZE > payload.len() { break; }
        let action_type = payload[offset];
        let data = &payload[offset+1..offset+1+258];
        let label_bytes = &payload[offset+1+258..offset+1+258+24];
        let label = String::from_utf8_lossy(label_bytes)
            .trim_end_matches('\0')
            .to_string();

        let mut action = ButtonAction {
            action_type,
            modifier: 0,
            keycode: 0,
            consumer_code: 0,
            text: String::new(),
            launcher_os: 0,
            label,
            macro_steps: Vec::new(),
            macro_count: 0,
        };

        match action_type {
            1 => { // KEY
                action.modifier = data[0];
                action.keycode = data[1];
            }
            2 => { // CONSUMER
                action.consumer_code = data[0] as u16 | ((data[1] as u16) << 8);
            }
            3 => { // MACRO
                action.macro_count = data[32]; // count at offset 32 (16 steps * 2 bytes)
                for s in 0..16 {
                    action.macro_steps.push(MacroStep {
                        modifier: data[s*2],
                        keycode: data[s*2+1],
                    });
                }
            }
            4 | 5 => { // TEXT / PASTE
                let len = data[256] as usize | ((data[257] as usize) << 8);
                action.text = String::from_utf8_lossy(&data[..len.min(256)])
                    .to_string();
            }
            6 => { // LAUNCHER
                action.launcher_os = data[0];
                action.text = String::from_utf8_lossy(&data[1..])
                    .trim_end_matches('\0')
                    .to_string();
            }
            _ => {}
        }

        buttons.push(action);
    }

    Ok(buttons)
}

#[tauri::command]
fn set_button(idx: usize, action: ButtonAction, state: State<AppState>) -> Result<(), String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;

    if idx >= NUM_BUTTONS { return Err("Button index out of range".into()); }

    let mut data = vec![0u8; ACTION_SIZE];
    data[0] = action.action_type;

    match action.action_type {
        1 => { // KEY
            data[1] = action.modifier;
            data[2] = action.keycode;
        }
        2 => { // CONSUMER
            data[1] = (action.consumer_code & 0xFF) as u8;
            data[2] = ((action.consumer_code >> 8) & 0xFF) as u8;
        }
        3 => { // MACRO
            for (i, step) in action.macro_steps.iter().enumerate().take(16) {
                data[1 + i*2] = step.modifier;
                data[1 + i*2 + 1] = step.keycode;
            }
            data[1 + 32] = action.macro_count;
        }
        4 | 5 => { // TEXT / PASTE
            let bytes = action.text.as_bytes();
            let len = bytes.len().min(255);
            data[1..1+len].copy_from_slice(&bytes[..len]);
            data[1+256] = (len & 0xFF) as u8;
            data[1+257] = ((len >> 8) & 0xFF) as u8;
        }
        6 => { // LAUNCHER
            data[1] = action.launcher_os;
            let name_bytes = action.text.as_bytes();
            let len = name_bytes.len().min(255);
            data[2..2+len].copy_from_slice(&name_bytes[..len]);
        }
        _ => {}
    }

    let mut payload = vec![idx as u8];
    payload.extend_from_slice(&data);

    send_command(port, CMD_SET_BUTTON, &payload)?;
    let reply = read_reply(port, REPLY_BUTTON_ACK)?;
    if reply.first() != Some(&(idx as u8)) {
        return Err("Device rejected button".into());
    }
    Ok(())
}

#[tauri::command]
fn save_flash(state: State<AppState>) -> Result<bool, String> {
    let mut port = state.port.lock().unwrap();
    let port = port.as_mut().ok_or("Not connected")?;

    send_command(port, CMD_SAVE_FLASH, &[])?;
    let reply = read_reply(port, REPLY_SAVE_ACK)?;
    Ok(reply.first() == Some(&1u8))
}

#[tauri::command]
fn launch_app_by_name(app_name: String) -> Result<(), String> {
    let app_name = app_name.trim();
    if app_name.is_empty() {
        return Err("No app name provided".into());
    }

    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .args(["-a", app_name])
            .spawn()
            .map_err(|e| format!("Failed to launch {}: {}", app_name, e))?;
    }

    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("cmd")
            .args(["/c", "start", "", app_name])
            .spawn()
            .map_err(|e| format!("Failed to launch {}: {}", app_name, e))?;
    }

    #[cfg(target_os = "linux")]
    {
        std::process::Command::new("xdg-open")
            .arg(app_name)
            .spawn()
            .map_err(|e| format!("Failed to launch {}: {}", app_name, e))?;
    }

    Ok(())
}

#[tauri::command]
fn list_installed_apps() -> Result<Vec<String>, String> {
    let mut apps = Vec::new();

    #[cfg(target_os = "macos")]
    {
        if let Ok(output) = std::process::Command::new("mdfind")
            .args(["kMDItemKind == 'Application'"])
            .output()
        {
            let stdout = String::from_utf8_lossy(&output.stdout);
            for line in stdout.lines() {
                if let Some(name) = std::path::Path::new(line)
                    .file_stem()
                    .and_then(|s| s.to_str())
                {
                    apps.push(name.to_string());
                }
            }
        }
    }

    #[cfg(target_os = "linux")]
    {
        if let Ok(output) = std::process::Command::new("ls")
            .arg("/usr/share/applications/")
            .output()
        {
            let stdout = String::from_utf8_lossy(&output.stdout);
            for line in stdout.lines() {
                if let Some(name) = line.strip_suffix(".desktop") {
                    apps.push(name.replace('-', " ").to_string());
                }
            }
        }
    }

    apps.sort();
    apps.dedup();
    Ok(apps)
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(AppState {
            port: Mutex::new(None),
            port_name: Mutex::new(String::new()),
        })
        .invoke_handler(tauri::generate_handler![
            list_ports,
            connect_port,
            disconnect_port,
            get_port,
            get_mapping,
            set_button,
            save_flash,
            launch_app_by_name,
            list_installed_apps,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
