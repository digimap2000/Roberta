import type {
  RawRegisterEntry,
  RegisterEntry,
  RegisterGroup,
} from "../models/robot";

type RegisterGroupDefinition = {
  id: string;
  label: string;
  description: string;
  members: string[];
};

const registerGroupDefinitions: RegisterGroupDefinition[] = [
  {
    id: "identity",
    label: "Identity",
    description: "Controller identity, protocol, and capability registers.",
    members: [
      "PROTOCOL_VERSION",
      "DEVICE_TYPE",
      "AXIS_ID",
      "CAPABILITY_WORD",
    ],
  },
  {
    id: "drive",
    label: "Drive",
    description: "Command, state, and fault words that govern motion enablement.",
    members: [
      "CONTROL_WORD",
      "STATUS_WORD",
      "FAULT_WORD",
      "MODE_WORD",
      "INPUT_WORD",
      "OUTPUT_WORD",
      "LAST_ERROR_CODE",
    ],
  },
  {
    id: "motion",
    label: "Motion",
    description: "Active and staged motion targets, timing, and live position telemetry.",
    members: [
      "STAGED_COMMAND_COUNTER",
      "ACTIVE_COMMAND_COUNTER",
      "EXECUTE_AT_TICK_US",
      "SEGMENT_DURATION_US",
      "STAGED_TARGET_POSITION_STEPS",
      "ACTIVE_TARGET_POSITION_STEPS",
      "CURRENT_POSITION_STEPS",
      "STAGED_TARGET_VELOCITY_STEPS_S",
      "ACTIVE_TARGET_VELOCITY_STEPS_S",
      "CURRENT_VELOCITY_STEPS_S",
      "STAGED_TARGET_ACCEL_STEPS_S2",
      "ACTIVE_TARGET_ACCEL_STEPS_S2",
      "STAGED_HOME_VELOCITY_STEPS_S",
      "ACTIVE_HOME_VELOCITY_STEPS_S",
      "HOME_OFFSET_STEPS",
      "STAGED_RELATIVE_MOVE_STEPS",
      "STAGED_JOINT_TARGET_DEG_Q12_20",
      "ACTIVE_JOINT_TARGET_DEG_Q12_20",
      "JOINT_POSITION_DEG_Q12_20",
      "STEP_COUNT",
      "UPTIME_MS",
      "LOCAL_TICK_US",
    ],
  },
  {
    id: "mechanics",
    label: "Mechanics",
    description: "Joint scaling, backlash, and software travel limits.",
    members: [
      "STEPS_PER_MOTOR_REV",
      "MOTOR_REV_PER_JOINT_REV_NUMERATOR",
      "MOTOR_REV_PER_JOINT_REV_DENOMINATOR",
      "JOINT_DEGREES_PER_REV_Q12_20",
      "JOINT_BACKLASH_DEG_Q12_20",
      "SOFT_LIMIT_MIN_STEPS",
      "SOFT_LIMIT_MAX_STEPS",
    ],
  },
  {
    id: "auxiliaries",
    label: "Auxiliaries",
    description: "Cooling, brake, and sync watchdog behavior.",
    members: [
      "FAN_LEVEL",
      "BRAKE_HOLD_ENABLE",
      "BRAKE_RELEASE_DELAY_MS",
      "BRAKE_APPLY_DELAY_MS",
      "LAST_SYNC_RX_TICK_US",
      "SYNC_WATCHDOG_TIMEOUT_MS",
      "SYNC_STATUS_WORD",
    ],
  },
];

function isHexRegister(name: string) {
  return (
    name.endsWith("_WORD") ||
    name.endsWith("_CODE") ||
    name.includes("CAPABILITY")
  );
}

function formatSigned(rawValue: number) {
  const signed = rawValue > 0x7fff_ffff ? rawValue - 0x1_0000_0000 : rawValue;
  return signed.toString();
}

function decodeQ12_20(rawValue: number) {
  const signed = rawValue > 0x7fff_ffff ? rawValue - 0x1_0000_0000 : rawValue;
  return signed / 2 ** 20;
}

function formatRegisterValue(entry: RawRegisterEntry) {
  const { name, value } = entry;

  if (name.includes("_Q12_20")) {
    return `${decodeQ12_20(value).toFixed(4)} deg`;
  }

  if (name.endsWith("_TICK_US")) {
    return `${formatSigned(value)} us`;
  }

  if (name.endsWith("_MS")) {
    return `${formatSigned(value)} ms`;
  }

  if (
    name.includes("_STEPS") ||
    name.includes("_VELOCITY_STEPS") ||
    name.includes("_ACCEL_STEPS") ||
    name === "STEP_COUNT"
  ) {
    return formatSigned(value);
  }

  if (isHexRegister(name)) {
    return entry.rawValue;
  }

  return formatSigned(value);
}

function toLabel(name: string) {
  return name
    .toLowerCase()
    .split("_")
    .map((segment) => segment[0]?.toUpperCase() + segment.slice(1))
    .join(" ");
}

function toDescription(name: string) {
  return `${toLabel(name)} register`;
}

function toRegisterEntry(entry: RawRegisterEntry): RegisterEntry {
  return {
    key: entry.name.toLowerCase(),
    label: toLabel(entry.name),
    value: formatRegisterValue(entry),
    rawValue: entry.rawValue,
    writable: entry.writable,
    description: toDescription(entry.name),
    changed: entry.changed,
  };
}

export function buildRegisterGroups(entries: RawRegisterEntry[]): RegisterGroup[] {
  const byName = new Map(entries.map((entry) => [entry.name, entry] as const));
  const consumed = new Set<string>();

  const groups = registerGroupDefinitions
    .map((definition) => {
      const registers = definition.members
        .map((member) => byName.get(member))
        .filter((entry): entry is RawRegisterEntry => Boolean(entry))
        .map((entry) => {
          consumed.add(entry.name);
          return toRegisterEntry(entry);
        });

      return {
        id: definition.id,
        label: definition.label,
        description: definition.description,
        registers,
      } satisfies RegisterGroup;
    })
    .filter((group) => group.registers.length > 0);

  const remaining = entries
    .filter((entry) => !consumed.has(entry.name))
    .map(toRegisterEntry);

  if (remaining.length > 0) {
    groups.push({
      id: "other",
      label: "Other",
      description: "Additional controller registers not yet grouped into a primary instrumentation band.",
      registers: remaining,
    });
  }

  return groups;
}
