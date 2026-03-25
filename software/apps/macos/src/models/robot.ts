export type WorkspaceSection =
  | "overview"
  | "axes"
  | "registers"
  | "diagnostics"
  | "experiments";

export type AxisIdentifier =
  | "axis-1"
  | "axis-2"
  | "axis-3"
  | "axis-4"
  | "axis-5"
  | "axis-6";

export type ConnectionMode = "disconnected" | "live";
export type AxisStatus =
  | "disconnected"
  | "idle"
  | "armed"
  | "jogging"
  | "fault"
  | "homing";
export type DiagnosticsLevel = "info" | "warn" | "fault";

export type ConnectionState = {
  mode: ConnectionMode;
  label: string;
  detail: string;
  latencyMs: number;
  pollHz: number;
};

export type AxisState = {
  id: AxisIdentifier;
  index: number;
  shortLabel: string;
  name: string;
  connected: boolean;
  devicePath: string | null;
  baudRate: number;
  controllerAxisId: number | null;
  status: AxisStatus;
  enabled: boolean;
  fault: string | null;
  positionDeg: number;
  targetPositionDeg: number;
  velocityDegPerSec: number;
  currentAmps: number;
  busVoltage: number;
  temperatureC: number;
  controlMode: "Position" | "Velocity";
  lastCommand: string;
};

export type RegisterEntry = {
  key: string;
  label: string;
  value: string;
  rawValue: string;
  writable: boolean;
  description: string;
  units?: string;
  changed: boolean;
};

export type RegisterGroup = {
  id: string;
  label: string;
  description: string;
  registers: RegisterEntry[];
};

export type DiagnosticsEvent = {
  id: string;
  level: DiagnosticsLevel;
  source: string;
  message: string;
  timestampLabel: string;
};

export type RawRegisterEntry = {
  address: number;
  name: string;
  value: number;
  rawValue: string;
  writable: boolean;
  changed: boolean;
};

export type RobotWorkspaceSnapshot = {
  timestamp: number;
  connection: ConnectionState;
  modeLabel: string;
  axes: AxisState[];
  diagnostics: DiagnosticsEvent[];
  availablePorts: string[];
};

export type WorkspaceSectionDefinition = {
  id: WorkspaceSection;
  label: string;
  summary: string;
};

export const workspaceSections: WorkspaceSectionDefinition[] = [
  {
    id: "overview",
    label: "Overview",
    summary: "System summary and workspace posture.",
  },
  {
    id: "axes",
    label: "Axes",
    summary: "Direct manipulation and live axis control.",
  },
  {
    id: "registers",
    label: "Registers",
    summary: "Dense instrumentation and register inspection.",
  },
  {
    id: "diagnostics",
    label: "Diagnostics",
    summary: "Transport health, faults, and recent events.",
  },
  {
    id: "experiments",
    label: "Experiments",
    summary: "Staging area for automation and AI-assisted tasks.",
  },
];
