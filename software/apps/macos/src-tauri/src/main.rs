#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod backend;

use std::sync::Mutex;

use backend::{AxisRegistersDto, WorkspaceBackend, WorkspaceSnapshotDto};

#[derive(serde::Deserialize)]
#[serde(rename_all = "camelCase")]
struct ConnectAxisPayload {
    axis_id: String,
    device_path: String,
    baud_rate: u32,
}

#[derive(serde::Deserialize)]
#[serde(rename_all = "camelCase")]
struct AxisActionPayload {
    axis_id: String,
}

#[derive(serde::Deserialize)]
#[serde(rename_all = "camelCase")]
struct JogAxisPayload {
    axis_id: String,
    direction: String,
    step_deg: f64,
}

#[tauri::command]
fn get_workspace_snapshot(state: tauri::State<'_, Mutex<WorkspaceBackend>>) -> WorkspaceSnapshotDto {
    state.lock().expect("workspace poisoned").workspace_snapshot()
}

#[tauri::command]
fn connect_axis(
    payload: ConnectAxisPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> WorkspaceSnapshotDto {
    state
        .lock()
        .expect("workspace poisoned")
        .connect_axis(&payload.axis_id, payload.device_path, payload.baud_rate)
}

#[tauri::command]
fn disconnect_axis(
    payload: AxisActionPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> WorkspaceSnapshotDto {
    state
        .lock()
        .expect("workspace poisoned")
        .disconnect_axis(&payload.axis_id)
}

#[tauri::command]
fn jog_axis(
    payload: JogAxisPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> WorkspaceSnapshotDto {
    state
        .lock()
        .expect("workspace poisoned")
        .jog_axis(&payload.axis_id, &payload.direction, payload.step_deg)
}

#[tauri::command]
fn clear_axis_fault(
    payload: AxisActionPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> WorkspaceSnapshotDto {
    state
        .lock()
        .expect("workspace poisoned")
        .clear_axis_fault(&payload.axis_id)
}

#[tauri::command]
fn set_axis_zero(
    payload: AxisActionPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> WorkspaceSnapshotDto {
    state
        .lock()
        .expect("workspace poisoned")
        .set_axis_zero(&payload.axis_id)
}

#[tauri::command]
fn get_axis_registers(
    payload: AxisActionPayload,
    state: tauri::State<'_, Mutex<WorkspaceBackend>>,
) -> AxisRegistersDto {
    state
        .lock()
        .expect("workspace poisoned")
        .axis_registers(&payload.axis_id)
}

fn main() {
    tauri::Builder::default()
        .manage(Mutex::new(WorkspaceBackend::default()))
        .invoke_handler(tauri::generate_handler![
            get_workspace_snapshot,
            connect_axis,
            disconnect_axis,
            jog_axis,
            clear_axis_fault,
            set_axis_zero,
            get_axis_registers,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
