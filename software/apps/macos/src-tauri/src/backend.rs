use std::collections::{HashMap, VecDeque};
use std::ffi::{c_void, CStr, CString};
use std::os::raw::{c_char, c_int};
use std::ptr::NonNull;

use chrono::Local;
use serde::Serialize;

const ERROR_BUFFER_LEN: usize = 512;
const MAX_DIAGNOSTICS: usize = 48;

const AXIS_LAYOUT: [(&str, &str, &str); 6] = [
    ("axis-1", "A1", "Base"),
    ("axis-2", "A2", "Shoulder"),
    ("axis-3", "A3", "Elbow"),
    ("axis-4", "A4", "Wrist pitch"),
    ("axis-5", "A5", "Wrist roll"),
    ("axis-6", "A6", "Tool"),
];

type RobertaAxisClientOpaque = c_void;

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct RobertaAxisSnapshotRaw {
    axis_id: u32,
    lifecycle: u32,
    status_word: u32,
    fault_word: u32,
    capability_word: u32,
    last_error_code: u32,
    current_position_steps: i32,
    target_position_steps: i32,
    current_velocity_steps_per_second: i32,
    step_count: i32,
    min_steps: i32,
    max_steps: i32,
    velocity_steps_per_second: i32,
    accel_steps_per_second_squared: i32,
    home_velocity_steps_per_second: i32,
    current_position_deg: f64,
    target_position_deg: f64,
    outputs_enabled: u8,
    motor_enabled: u8,
    homed: u8,
    moving: u8,
    homing: u8,
    limit_home: u8,
    limit_a: u8,
    limit_b: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct RobertaAxisRegisterValueRaw {
    address: u16,
    value: u32,
}

unsafe extern "C" {
    fn roberta_axis_client_new(device_path: *const c_char, baud_rate: c_int) -> *mut RobertaAxisClientOpaque;
    fn roberta_axis_client_free(client: *mut RobertaAxisClientOpaque);
    fn roberta_axis_client_connect(
        client: *mut RobertaAxisClientOpaque,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_client_disconnect(client: *mut RobertaAxisClientOpaque);
    fn roberta_axis_client_is_connected(client: *const RobertaAxisClientOpaque) -> u8;
    fn roberta_axis_client_snapshot(
        client: *mut RobertaAxisClientOpaque,
        out_snapshot: *mut RobertaAxisSnapshotRaw,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_client_enable_drive(
        client: *mut RobertaAxisClientOpaque,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_client_jog_relative_degrees(
        client: *mut RobertaAxisClientOpaque,
        delta_degrees: f64,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_client_clear_faults(
        client: *mut RobertaAxisClientOpaque,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_client_set_zero(
        client: *mut RobertaAxisClientOpaque,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
    fn roberta_axis_register_count() -> usize;
    fn roberta_axis_register_name(reg_address: u16) -> *const c_char;
    fn roberta_axis_register_is_writable(reg_address: u16) -> u8;
    fn roberta_axis_client_read_all_registers(
        client: *mut RobertaAxisClientOpaque,
        out_values: *mut RobertaAxisRegisterValueRaw,
        capacity: usize,
        out_written: *mut usize,
        error_buffer: *mut c_char,
        error_buffer_len: usize,
    ) -> u8;
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DiagnosticsEventDto {
    pub id: String,
    pub level: String,
    pub source: String,
    pub message: String,
    pub timestamp_label: String,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ConnectionStateDto {
    pub mode: String,
    pub label: String,
    pub detail: String,
    pub latency_ms: u32,
    pub poll_hz: u32,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct AxisStateDto {
    pub id: String,
    pub index: u32,
    pub short_label: String,
    pub name: String,
    pub connected: bool,
    pub device_path: Option<String>,
    pub baud_rate: u32,
    pub controller_axis_id: Option<u32>,
    pub status: String,
    pub enabled: bool,
    pub fault: Option<String>,
    pub position_deg: f64,
    pub target_position_deg: f64,
    pub velocity_deg_per_sec: f64,
    pub current_amps: f64,
    pub bus_voltage: f64,
    pub temperature_c: f64,
    pub control_mode: String,
    pub last_command: String,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RawRegisterDto {
    pub address: u16,
    pub name: String,
    pub value: u32,
    pub raw_value: String,
    pub writable: bool,
    pub changed: bool,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct AxisRegistersDto {
    pub axis_id: String,
    pub entries: Vec<RawRegisterDto>,
    pub error: Option<String>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct WorkspaceSnapshotDto {
    pub timestamp: u64,
    pub connection: ConnectionStateDto,
    pub mode_label: String,
    pub axes: Vec<AxisStateDto>,
    pub diagnostics: Vec<DiagnosticsEventDto>,
    pub available_ports: Vec<String>,
}

struct AxisClient {
    raw: NonNull<RobertaAxisClientOpaque>,
}

unsafe impl Send for AxisClient {}

impl AxisClient {
    fn new(device_path: &str, baud_rate: u32) -> Result<Self, String> {
        let device_path = CString::new(device_path).map_err(|_| "device path contains an interior NUL byte".to_string())?;
        let raw = unsafe { roberta_axis_client_new(device_path.as_ptr(), baud_rate as c_int) };
        let raw = NonNull::new(raw).ok_or_else(|| "failed to allocate axis client".to_string())?;
        Ok(Self { raw })
    }

    fn connect(&mut self) -> Result<(), String> {
        call_status(|buffer, len| unsafe {
            roberta_axis_client_connect(self.raw.as_ptr(), buffer, len)
        })
    }

    fn is_connected(&self) -> bool {
        unsafe { roberta_axis_client_is_connected(self.raw.as_ptr()) != 0 }
    }

    fn snapshot(&mut self) -> Result<RobertaAxisSnapshotRaw, String> {
        let mut snapshot = RobertaAxisSnapshotRaw::default();
        call_status(|buffer, len| unsafe {
            roberta_axis_client_snapshot(self.raw.as_ptr(), &mut snapshot, buffer, len)
        })?;
        Ok(snapshot)
    }

    fn enable_drive(&mut self) -> Result<(), String> {
        call_status(|buffer, len| unsafe {
            roberta_axis_client_enable_drive(self.raw.as_ptr(), buffer, len)
        })
    }

    fn jog_relative_degrees(&mut self, delta_degrees: f64) -> Result<(), String> {
        call_status(|buffer, len| unsafe {
            roberta_axis_client_jog_relative_degrees(self.raw.as_ptr(), delta_degrees, buffer, len)
        })
    }

    fn clear_faults(&mut self) -> Result<(), String> {
        call_status(|buffer, len| unsafe {
            roberta_axis_client_clear_faults(self.raw.as_ptr(), buffer, len)
        })
    }

    fn set_zero(&mut self) -> Result<(), String> {
        call_status(|buffer, len| unsafe {
            roberta_axis_client_set_zero(self.raw.as_ptr(), buffer, len)
        })
    }

    fn read_all_registers(&mut self) -> Result<Vec<RobertaAxisRegisterValueRaw>, String> {
        let register_count = unsafe { roberta_axis_register_count() };
        let mut values = vec![RobertaAxisRegisterValueRaw::default(); register_count];
        let mut written = 0usize;
        call_status(|buffer, len| unsafe {
            roberta_axis_client_read_all_registers(
                self.raw.as_ptr(),
                values.as_mut_ptr(),
                values.len(),
                &mut written,
                buffer,
                len,
            )
        })?;
        values.truncate(written);
        Ok(values)
    }

    fn disconnect(&mut self) {
        unsafe { roberta_axis_client_disconnect(self.raw.as_ptr()) };
    }
}

impl Drop for AxisClient {
    fn drop(&mut self) {
        unsafe { roberta_axis_client_free(self.raw.as_ptr()) };
    }
}

fn call_status<F>(call: F) -> Result<(), String>
where
    F: FnOnce(*mut c_char, usize) -> u8,
{
    let mut error_buffer = vec![0u8; ERROR_BUFFER_LEN];
    let ok = call(error_buffer.as_mut_ptr().cast(), error_buffer.len()) != 0;
    if ok {
        return Ok(());
    }

    let error = unsafe { CStr::from_ptr(error_buffer.as_ptr().cast()) }
        .to_string_lossy()
        .trim()
        .to_string();

    if error.is_empty() {
        Err("native command failed".to_string())
    } else {
        Err(error)
    }
}

struct AxisSlot {
    id: &'static str,
    short_label: &'static str,
    name: &'static str,
    index: u32,
    device_path: Option<String>,
    baud_rate: u32,
    client: Option<AxisClient>,
    last_command: String,
    last_error: Option<String>,
    last_register_values: HashMap<u16, u32>,
}

impl AxisSlot {
    fn disconnected(id: &'static str, short_label: &'static str, name: &'static str, index: u32) -> Self {
        Self {
            id,
            short_label,
            name,
            index,
            device_path: None,
            baud_rate: 115_200,
            client: None,
            last_command: "Awaiting controller connection".to_string(),
            last_error: None,
            last_register_values: HashMap::new(),
        }
    }
}

struct PendingEvent {
    level: &'static str,
    source: String,
    message: String,
}

pub struct WorkspaceBackend {
    axes: Vec<AxisSlot>,
    diagnostics: VecDeque<DiagnosticsEventDto>,
    next_event_id: u64,
}

impl Default for WorkspaceBackend {
    fn default() -> Self {
        Self {
            axes: AXIS_LAYOUT
                .iter()
                .enumerate()
                .map(|(index, (id, short_label, name))| {
                    AxisSlot::disconnected(id, short_label, name, (index + 1) as u32)
                })
                .collect(),
            diagnostics: VecDeque::new(),
            next_event_id: 1,
        }
    }
}

impl WorkspaceBackend {
    pub fn workspace_snapshot(&mut self) -> WorkspaceSnapshotDto {
        let mut axes = Vec::with_capacity(self.axes.len());
        let mut pending_events = Vec::new();

        for axis in &mut self.axes {
            let (axis_state, pending_event) = axis_state(axis);
            axes.push(axis_state);
            if let Some(pending_event) = pending_event {
                pending_events.push(pending_event);
            }
        }

        for pending_event in pending_events {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        let live_count = axes.iter().filter(|axis| axis.connected).count();
        let available_ports = list_serial_ports();
        let connection = if live_count == 0 {
            ConnectionStateDto {
                mode: "disconnected".to_string(),
                label: "Disconnected".to_string(),
                detail: "No live serial-console axis controllers are connected.".to_string(),
                latency_ms: 0,
                poll_hz: 1,
            }
        } else {
            ConnectionStateDto {
                mode: "live".to_string(),
                label: format!("Live ({live_count})"),
                detail: format!("{live_count} axis controller(s) connected through the C++ core."),
                latency_ms: 40,
                poll_hz: 4,
            }
        };

        WorkspaceSnapshotDto {
            timestamp: now_millis(),
            connection,
            mode_label: if live_count == 0 {
                "Disconnected".to_string()
            } else {
                "Live serial console".to_string()
            },
            axes,
            diagnostics: self.diagnostics.iter().cloned().collect(),
            available_ports,
        }
    }

    pub fn connect_axis(&mut self, axis_id: &str, device_path: String, baud_rate: u32) -> WorkspaceSnapshotDto {
        let pending_event = match self.find_axis_mut(axis_id) {
            Some(axis) => {
                axis.device_path = Some(device_path.clone());
                axis.baud_rate = baud_rate;
                axis.last_error = None;

                match AxisClient::new(&device_path, baud_rate).and_then(|mut client| {
                    client.connect()?;
                    Ok(client)
                }) {
                    Ok(client) => {
                        axis.client = Some(client);
                        axis.last_command = "Controller connected".to_string();
                        Some(PendingEvent {
                            level: "info",
                            source: axis.short_label.to_string(),
                            message: format!("Connected {}", device_path),
                        })
                    }
                    Err(error) => {
                        axis.client = None;
                        axis.last_command = "Connection failed".to_string();
                        axis.last_error = Some(error.clone());
                        Some(PendingEvent {
                            level: "fault",
                            source: axis.short_label.to_string(),
                            message: format!("Connection failed: {error}"),
                        })
                    }
                }
            }
            None => Some(PendingEvent {
                level: "fault",
                source: "Workspace".to_string(),
                message: format!("Unknown axis id {axis_id}"),
            }),
        };

        if let Some(pending_event) = pending_event {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        self.workspace_snapshot()
    }

    pub fn disconnect_axis(&mut self, axis_id: &str) -> WorkspaceSnapshotDto {
        let mut pending_event = None;
        if let Some(axis) = self.find_axis_mut(axis_id) {
            if let Some(client) = axis.client.as_mut() {
                client.disconnect();
            }
            axis.client = None;
            axis.last_register_values.clear();
            axis.last_error = None;
            axis.last_command = "Controller disconnected".to_string();
            pending_event = Some(PendingEvent {
                level: "warn",
                source: axis.short_label.to_string(),
                message: "Controller disconnected".to_string(),
            });
        }

        if let Some(pending_event) = pending_event {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        self.workspace_snapshot()
    }

    pub fn jog_axis(&mut self, axis_id: &str, direction: &str, step_deg: f64) -> WorkspaceSnapshotDto {
        let delta = if direction == "negative" { -step_deg } else { step_deg };

        let mut pending_event = None;
        if let Some(axis) = self.find_axis_mut(axis_id) {
            match axis.client.as_mut() {
                Some(client) => {
                    let result = client
                        .enable_drive()
                        .and_then(|_| client.jog_relative_degrees(delta));

                    match result {
                        Ok(()) => {
                            axis.last_error = None;
                            axis.last_command = format!("Step jog {:+.1} deg", delta);
                            pending_event = Some(PendingEvent {
                                level: "info",
                                source: axis.short_label.to_string(),
                                message: format!("Jog command {:+.1} deg issued", delta),
                            });
                        }
                        Err(error) => {
                            axis.last_error = Some(error.clone());
                            axis.last_command = "Jog failed".to_string();
                            pending_event = Some(PendingEvent {
                                level: "fault",
                                source: axis.short_label.to_string(),
                                message: format!("Jog failed: {error}"),
                            });
                        }
                    }
                }
                None => {
                    pending_event = Some(PendingEvent {
                        level: "warn",
                        source: axis.short_label.to_string(),
                        message: "Jog ignored while disconnected".to_string(),
                    });
                }
            }
        }

        if let Some(pending_event) = pending_event {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        self.workspace_snapshot()
    }

    pub fn clear_axis_fault(&mut self, axis_id: &str) -> WorkspaceSnapshotDto {
        let mut pending_event = None;
        if let Some(axis) = self.find_axis_mut(axis_id) {
            match axis.client.as_mut() {
                Some(client) => match client.clear_faults() {
                    Ok(()) => {
                        axis.last_error = None;
                        axis.last_command = "Clear fault requested".to_string();
                        pending_event = Some(PendingEvent {
                            level: "info",
                            source: axis.short_label.to_string(),
                            message: "Fault clear requested".to_string(),
                        });
                    }
                    Err(error) => {
                        axis.last_error = Some(error.clone());
                        axis.last_command = "Fault clear failed".to_string();
                        pending_event = Some(PendingEvent {
                            level: "fault",
                            source: axis.short_label.to_string(),
                            message: format!("Fault clear failed: {error}"),
                        });
                    }
                },
                None => {
                    pending_event = Some(PendingEvent {
                        level: "warn",
                        source: axis.short_label.to_string(),
                        message: "Fault clear ignored while disconnected".to_string(),
                    });
                }
            }
        }

        if let Some(pending_event) = pending_event {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        self.workspace_snapshot()
    }

    pub fn set_axis_zero(&mut self, axis_id: &str) -> WorkspaceSnapshotDto {
        let mut pending_event = None;
        if let Some(axis) = self.find_axis_mut(axis_id) {
            match axis.client.as_mut() {
                Some(client) => match client.set_zero() {
                    Ok(()) => {
                        axis.last_error = None;
                        axis.last_command = "Set zero requested".to_string();
                        pending_event = Some(PendingEvent {
                            level: "info",
                            source: axis.short_label.to_string(),
                            message: "Set zero requested".to_string(),
                        });
                    }
                    Err(error) => {
                        axis.last_error = Some(error.clone());
                        axis.last_command = "Set zero failed".to_string();
                        pending_event = Some(PendingEvent {
                            level: "fault",
                            source: axis.short_label.to_string(),
                            message: format!("Set zero failed: {error}"),
                        });
                    }
                },
                None => {
                    pending_event = Some(PendingEvent {
                        level: "warn",
                        source: axis.short_label.to_string(),
                        message: "Set zero ignored while disconnected".to_string(),
                    });
                }
            }
        }

        if let Some(pending_event) = pending_event {
            self.push_event(pending_event.level, &pending_event.source, pending_event.message);
        }

        self.workspace_snapshot()
    }

    pub fn axis_registers(&mut self, axis_id: &str) -> AxisRegistersDto {
        match self.find_axis_mut(axis_id) {
            Some(axis) => match axis.client.as_mut() {
                Some(client) => match client.read_all_registers() {
                    Ok(values) => {
                        let entries = values
                            .into_iter()
                            .map(|entry| {
                                let previous = axis.last_register_values.insert(entry.address, entry.value);
                                RawRegisterDto {
                                    address: entry.address,
                                    name: register_name(entry.address),
                                    value: entry.value,
                                    raw_value: format!("0x{:08X}", entry.value),
                                    writable: register_writable(entry.address),
                                    changed: previous.map(|value| value != entry.value).unwrap_or(false),
                                }
                            })
                            .collect();

                        AxisRegistersDto {
                            axis_id: axis.id.to_string(),
                            entries,
                            error: None,
                        }
                    }
                    Err(error) => {
                        axis.last_error = Some(error.clone());
                        let pending_event = PendingEvent {
                            level: "fault",
                            source: axis.short_label.to_string(),
                            message: format!("Register read failed: {error}"),
                        };
                        let result = AxisRegistersDto {
                            axis_id: axis.id.to_string(),
                            entries: Vec::new(),
                            error: Some(error),
                        };
                        self.push_event(pending_event.level, &pending_event.source, pending_event.message);
                        result
                    }
                },
                None => AxisRegistersDto {
                    axis_id: axis.id.to_string(),
                    entries: Vec::new(),
                    error: Some("Axis controller is disconnected".to_string()),
                },
            },
            None => AxisRegistersDto {
                axis_id: axis_id.to_string(),
                entries: Vec::new(),
                error: Some("Unknown axis".to_string()),
            },
        }
    }

    fn find_axis_mut(&mut self, axis_id: &str) -> Option<&mut AxisSlot> {
        self.axes.iter_mut().find(|axis| axis.id == axis_id)
    }

    fn push_event(&mut self, level: &str, source: &str, message: String) {
        let event = DiagnosticsEventDto {
            id: format!("diag-{}", self.next_event_id),
            level: level.to_string(),
            source: source.to_string(),
            message,
            timestamp_label: Local::now().format("%H:%M:%S").to_string(),
        };
        self.next_event_id += 1;
        self.diagnostics.push_front(event);
        while self.diagnostics.len() > MAX_DIAGNOSTICS {
            self.diagnostics.pop_back();
        }
    }
}

fn disconnected_axis_state(axis: &AxisSlot, fault: Option<String>) -> AxisStateDto {
    AxisStateDto {
        id: axis.id.to_string(),
        index: axis.index,
        short_label: axis.short_label.to_string(),
        name: axis.name.to_string(),
        connected: false,
        device_path: axis.device_path.clone(),
        baud_rate: axis.baud_rate,
        controller_axis_id: None,
        status: "disconnected".to_string(),
        enabled: false,
        fault,
        position_deg: 0.0,
        target_position_deg: 0.0,
        velocity_deg_per_sec: 0.0,
        current_amps: 0.0,
        bus_voltage: 0.0,
        temperature_c: 0.0,
        control_mode: "Position".to_string(),
        last_command: axis.last_command.clone(),
    }
}

fn register_name(address: u16) -> String {
    let pointer = unsafe { roberta_axis_register_name(address) };
    if pointer.is_null() {
        return "UNKNOWN_REGISTER".to_string();
    }

    unsafe { CStr::from_ptr(pointer) }
        .to_string_lossy()
        .into_owned()
}

fn register_writable(address: u16) -> bool {
    unsafe { roberta_axis_register_is_writable(address) != 0 }
}

fn list_serial_ports() -> Vec<String> {
    let mut ports = serialport::available_ports()
        .map(|ports| ports.into_iter().map(|port| port.port_name).collect::<Vec<_>>())
        .unwrap_or_default();
    ports.sort();
    ports
}

fn axis_status(snapshot: &RobertaAxisSnapshotRaw) -> &'static str {
    if snapshot.fault_word != 0 {
        "fault"
    } else if snapshot.homing != 0 {
        "homing"
    } else if snapshot.moving != 0 {
        "jogging"
    } else if snapshot.outputs_enabled != 0 && snapshot.motor_enabled != 0 {
        "armed"
    } else {
        "idle"
    }
}

fn axis_fault(snapshot: &RobertaAxisSnapshotRaw) -> Option<String> {
    if snapshot.fault_word == 0 {
        None
    } else {
        Some(format!("Fault 0x{:08X}", snapshot.fault_word))
    }
}

fn steps_velocity_to_degrees(steps_per_second: i32, position_steps: i32, position_deg: f64) -> f64 {
    if position_steps == 0 || position_deg.abs() < f64::EPSILON {
        return 0.0;
    }

    let steps_per_degree = position_steps as f64 / position_deg;
    if !steps_per_degree.is_finite() || steps_per_degree.abs() < f64::EPSILON {
        return 0.0;
    }

    steps_per_second as f64 / steps_per_degree
}

fn now_millis() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or_default()
}

fn axis_state(axis: &mut AxisSlot) -> (AxisStateDto, Option<PendingEvent>) {
    match axis.client.as_mut() {
        Some(client) if client.is_connected() => match client.snapshot() {
            Ok(snapshot) => {
                axis.last_error = None;
                (
                    AxisStateDto {
                        id: axis.id.to_string(),
                        index: axis.index,
                        short_label: axis.short_label.to_string(),
                        name: axis.name.to_string(),
                        connected: true,
                        device_path: axis.device_path.clone(),
                        baud_rate: axis.baud_rate,
                        controller_axis_id: Some(snapshot.axis_id),
                        status: axis_status(&snapshot).to_string(),
                        enabled: snapshot.outputs_enabled != 0 && snapshot.motor_enabled != 0,
                        fault: axis_fault(&snapshot),
                        position_deg: snapshot.current_position_deg,
                        target_position_deg: snapshot.target_position_deg,
                        velocity_deg_per_sec: steps_velocity_to_degrees(
                            snapshot.current_velocity_steps_per_second,
                            snapshot.current_position_steps,
                            snapshot.current_position_deg,
                        ),
                        current_amps: 0.0,
                        bus_voltage: 0.0,
                        temperature_c: 0.0,
                        control_mode: "Position".to_string(),
                        last_command: axis.last_command.clone(),
                    },
                    None,
                )
            }
            Err(error) => {
                axis.last_error = Some(error.clone());
                axis.last_command = "Communication fault".to_string();
                if let Some(client) = axis.client.as_mut() {
                    client.disconnect();
                }
                axis.client = None;

                (
                    disconnected_axis_state(axis, Some(error.clone())),
                    Some(PendingEvent {
                        level: "fault",
                        source: axis.short_label.to_string(),
                        message: format!("Snapshot failed: {error}"),
                    }),
                )
            }
        },
        _ => (disconnected_axis_state(axis, axis.last_error.clone()), None),
    }
}
