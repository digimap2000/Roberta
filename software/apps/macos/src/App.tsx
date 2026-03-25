import { useMemo } from "react";
import {
  Activity,
  Bot,
  ChevronLeft,
  ChevronRight,
  Command,
  LayoutDashboard,
  Menu,
  RotateCcw,
  Settings2,
  SlidersHorizontal,
  Waypoints,
} from "lucide-react";

import {
  AppContent,
  AppHeader,
  AppShell,
  AppSidebar,
  type AppSidebarSection,
  SettingsPanel,
  SettingsSection,
  ShellPanelLayout,
  ShellSettings,
  StatusBar,
  useShellShortcuts,
  useShellState,
} from "@yourorg/app-shell";
import { Button, ThemeProvider } from "@yourorg/ui";

import { CommandPalette, type CommandPaletteCommand } from "./components/command-palette";
import { useRobertaWorkspace } from "./hooks/use-roberta-workspace";
import type {
  AxisState,
  DiagnosticsEvent,
  RegisterEntry,
  RegisterGroup,
  WorkspaceSection,
} from "./models/robot";
import { workspaceSections } from "./models/robot";

const sectionIcons = {
  overview: LayoutDashboard,
  axes: Waypoints,
  registers: SlidersHorizontal,
  diagnostics: Activity,
  experiments: Bot,
} satisfies Record<WorkspaceSection, typeof LayoutDashboard>;

const jogStepOptions = [0.1, 1, 5];

function sectionLabel(section: WorkspaceSection) {
  return workspaceSections.find((entry) => entry.id === section)?.label ?? "Workspace";
}

function formatSigned(value: number, units: string) {
  const sign = value > 0 ? "+" : "";
  return `${sign}${value.toFixed(2)} ${units}`;
}

function axisStatusTone(axis: AxisState) {
  if (!axis.connected) {
    return "muted";
  }

  if (axis.fault) {
    return "fault";
  }

  switch (axis.status) {
    case "jogging":
      return "active";
    case "homing":
      return "warn";
    case "idle":
      return "muted";
    default:
      return "ok";
  }
}

function levelTone(level: DiagnosticsEvent["level"]) {
  switch (level) {
    case "fault":
      return "fault";
    case "warn":
      return "warn";
    default:
      return "ok";
  }
}

function HeaderChips({
  activeSection,
  connectionLabel,
  connectionMode,
  modeLabel,
  selectedAxis,
}: {
  activeSection: WorkspaceSection;
  connectionLabel: string;
  connectionMode: "disconnected" | "live";
  modeLabel: string;
  selectedAxis: AxisState | null;
}) {
  return (
    <div className="roberta-header-chips">
      <span
        className={`roberta-chip roberta-chip--${
          connectionMode === "live" ? "ok" : "muted"
        }`}
      >
        {connectionLabel}
      </span>
      <span className="roberta-chip">{modeLabel}</span>
      <span className="roberta-chip">{sectionLabel(activeSection)}</span>
      {selectedAxis ? (
        <span className={`roberta-chip roberta-chip--${axisStatusTone(selectedAxis)}`}>
          {selectedAxis.shortLabel}
          {selectedAxis.connected
            ? selectedAxis.fault
              ? " faulted"
              : ` ${selectedAxis.status}`
            : " disconnected"}
        </span>
      ) : null}
    </div>
  );
}

function RobertaHeader({
  activeSection,
  connectionLabel,
  connectionMode,
  modeLabel,
  onOpenCommandPalette,
  selectedAxis,
}: {
  activeSection: WorkspaceSection;
  connectionLabel: string;
  connectionMode: "disconnected" | "live";
  modeLabel: string;
  onOpenCommandPalette: () => void;
  selectedAxis: AxisState | null;
}) {
  const { isMobile, sidebarCollapsed, toggleSettings, toggleSidebar } = useShellState();

  return (
    <AppHeader
      breadcrumbs={<span>Robot development workspace</span>}
      leading={
        <Button
          aria-label={
            isMobile
              ? "Open navigation"
              : sidebarCollapsed
                ? "Expand navigation"
                : "Collapse navigation"
          }
          onClick={toggleSidebar}
          size="icon"
          variant="outline"
        >
          {isMobile ? (
            <Menu className="h-4 w-4" />
          ) : sidebarCollapsed ? (
            <ChevronRight className="h-4 w-4" />
          ) : (
            <ChevronLeft className="h-4 w-4" />
          )}
        </Button>
      }
      title="Roberta"
      subtitle="Continuous robot control and inspection"
      actions={
        <div className="roberta-header-actions">
          <HeaderChips
            activeSection={activeSection}
            connectionLabel={connectionLabel}
            connectionMode={connectionMode}
            modeLabel={modeLabel}
            selectedAxis={selectedAxis}
          />
          <Button
            aria-label="Open command palette"
            onClick={onOpenCommandPalette}
            size="sm"
            variant="outline"
          >
            <Command className="h-4 w-4" />
            Command
          </Button>
          <Button
            aria-label="Open shell settings"
            onClick={toggleSettings}
            size="icon"
            variant="outline"
          >
            <Settings2 className="h-4 w-4" />
          </Button>
        </div>
      }
    />
  );
}

function RobertaSidebar({
  activeSection,
  onSelectSection,
}: {
  activeSection: WorkspaceSection;
  onSelectSection: (section: WorkspaceSection) => void;
}) {
  const sections = useMemo<AppSidebarSection[]>(
    () => [
      {
        id: "workspace",
        items: workspaceSections.map((section) => ({
          id: section.id,
          label: section.label,
          caption: section.summary,
          icon: sectionIcons[section.id],
          active: section.id === activeSection,
          onSelect: () => onSelectSection(section.id),
        })),
      },
    ],
    [activeSection, onSelectSection],
  );

  return (
    <AppSidebar
      brand={
        <div className="roberta-brand">
          <strong className="roberta-brand__title">Roberta</strong>
          <span className="roberta-brand__subtitle">
            Workspace-first robot development
          </span>
        </div>
      }
      sections={sections}
    />
  );
}

function RobertaSettings({
  availablePorts,
  axes,
  axisConnectionDrafts,
  connectionDetail,
  diagnosticsDockOpen,
  modeLabel,
  onConnectAxis,
  onDisconnectAxis,
  onUpdateAxisConnectionDraft,
}: {
  availablePorts: string[];
  axes: AxisState[];
  axisConnectionDrafts: Partial<
    Record<
      AxisState["id"],
      {
        devicePath: string;
        baudRate: number;
      }
    >
  >;
  connectionDetail: string;
  diagnosticsDockOpen: boolean;
  modeLabel: string;
  onConnectAxis: (axisId: AxisState["id"]) => void;
  onDisconnectAxis: (axisId: AxisState["id"]) => void;
  onUpdateAxisConnectionDraft: (
    axisId: AxisState["id"],
    draft: Partial<{ devicePath: string; baudRate: number }>,
  ) => void;
}) {
  return (
    <SettingsPanel title="Workspace settings">
      <ShellSettings />
      <SettingsSection
        description="Roberta-specific runtime posture lives here, not in the shared shell package."
        title="Runtime"
      >
        <div className="roberta-settings-grid">
          <div>
            <span className="roberta-settings-label">Connection</span>
            <p>{connectionDetail}</p>
          </div>
          <div>
            <span className="roberta-settings-label">Mode</span>
            <p>{modeLabel}</p>
          </div>
          <div>
            <span className="roberta-settings-label">Diagnostics dock</span>
            <p>{diagnosticsDockOpen ? "Expanded" : "Reserved and collapsed"}</p>
          </div>
          <div>
            <span className="roberta-settings-label">Command palette</span>
            <p>`Cmd/Ctrl+K` opens workspace actions.</p>
          </div>
        </div>
      </SettingsSection>
      <SettingsSection
        description="Map workspace axes onto real serial-console controllers exposed by the C++ core."
        title="Axis controllers"
      >
        <div className="roberta-axis-config-list">
          {axes.map((axis) => {
            const draft = axisConnectionDrafts[axis.id] ?? {
              devicePath: axis.devicePath ?? "",
              baudRate: axis.baudRate,
            };

            return (
              <div className="roberta-axis-config-row" key={axis.id}>
                <div className="roberta-axis-config-row__identity">
                  <strong>{axis.shortLabel}</strong>
                  <span>{axis.name}</span>
                </div>
                <div className="roberta-axis-config-row__fields">
                  <label className="roberta-config-field">
                    <span className="roberta-settings-label">Device path</span>
                    <input
                      aria-label={`${axis.shortLabel} device path`}
                      className="roberta-config-input"
                      list={`axis-port-options-${axis.id}`}
                      onChange={(event) =>
                        onUpdateAxisConnectionDraft(axis.id, {
                          devicePath: event.target.value,
                        })}
                      placeholder="/dev/cu.usbmodem..."
                      type="text"
                      value={draft.devicePath}
                    />
                  </label>
                  <datalist id={`axis-port-options-${axis.id}`}>
                    {availablePorts.map((port) => (
                      <option key={port} value={port} />
                    ))}
                  </datalist>
                  <label className="roberta-config-field roberta-config-field--baud">
                    <span className="roberta-settings-label">Baud rate</span>
                    <input
                      aria-label={`${axis.shortLabel} baud rate`}
                      className="roberta-config-input roberta-config-input--baud"
                      min={1200}
                      onChange={(event) =>
                        onUpdateAxisConnectionDraft(axis.id, {
                          baudRate: Number.parseInt(event.target.value, 10) || 115200,
                        })}
                      type="number"
                      value={draft.baudRate}
                    />
                  </label>
                </div>
                <div className="roberta-axis-config-row__actions">
                  {axis.connected ? (
                    <Button
                      onClick={() => onDisconnectAxis(axis.id)}
                      size="sm"
                      variant="outline"
                    >
                      Disconnect
                    </Button>
                  ) : (
                    <Button
                      onClick={() => onConnectAxis(axis.id)}
                      size="sm"
                      variant="outline"
                    >
                      Connect
                    </Button>
                  )}
                </div>
              </div>
            );
          })}
        </div>
      </SettingsSection>
    </SettingsPanel>
  );
}

function SectionHeading({
  eyebrow,
  title,
  summary,
}: {
  eyebrow: string;
  title: string;
  summary: string;
}) {
  return (
    <header className="roberta-section-heading">
      <p className="roberta-eyebrow">{eyebrow}</p>
      <h2>{title}</h2>
      <p>{summary}</p>
    </header>
  );
}

function AxisMatrix({
  axes,
  selectedAxisId,
  onSelectAxis,
}: {
  axes: AxisState[];
  selectedAxisId: string;
  onSelectAxis: (axisId: AxisState["id"]) => void;
}) {
  return (
    <div className="roberta-axis-matrix">
      {axes.map((axis) => (
        <button
          className={`roberta-axis-tile ${
            axis.id === selectedAxisId ? "is-selected" : ""
          }`}
          key={axis.id}
          onClick={() => onSelectAxis(axis.id)}
          type="button"
        >
          <div className="roberta-axis-tile__head">
            <span className="roberta-axis-tile__label">{axis.shortLabel}</span>
            <span className={`roberta-state-dot roberta-state-dot--${axisStatusTone(axis)}`} />
          </div>
          <div className="roberta-axis-tile__title">{axis.name}</div>
          <div className="roberta-axis-tile__value">
            {axis.connected ? `${axis.positionDeg.toFixed(2)} deg` : "Offline"}
          </div>
          <div className="roberta-axis-tile__meta">
            <span>{axis.connected ? `${axis.velocityDegPerSec.toFixed(2)} deg/s` : "No link"}</span>
            <span>{axis.fault ?? axis.status}</span>
          </div>
        </button>
      ))}
    </div>
  );
}

function AxisRail({
  axes,
  selectedAxisId,
  onSelectAxis,
}: {
  axes: AxisState[];
  selectedAxisId: string;
  onSelectAxis: (axisId: AxisState["id"]) => void;
}) {
  return (
    <aside className="roberta-axis-rail" aria-label="Axis list">
      <div className="roberta-axis-rail__header">
        <span>All axes</span>
        <span>{axes.length} total</span>
      </div>
      <div className="roberta-axis-rail__list">
        {axes.map((axis) => (
          <button
            className={`roberta-axis-row ${
              axis.id === selectedAxisId ? "is-selected" : ""
            }`}
            key={axis.id}
            onClick={() => onSelectAxis(axis.id)}
            type="button"
          >
            <span className={`roberta-axis-row__accent roberta-axis-row__accent--${axisStatusTone(axis)}`} />
            <div className="roberta-axis-row__identity">
              <span className="roberta-axis-row__id">{axis.shortLabel}</span>
              <span className="roberta-axis-row__name">{axis.name}</span>
            </div>
            <span className="roberta-axis-row__position">
              {axis.connected ? `${axis.positionDeg.toFixed(2)} deg` : "offline"}
            </span>
            {axis.fault ? (
              <span className="roberta-axis-row__fault" title={axis.fault}>
                fault
              </span>
            ) : null}
            <span className={`roberta-state-dot roberta-state-dot--${axisStatusTone(axis)}`} />
          </button>
        ))}
      </div>
    </aside>
  );
}

function MetricStrip({ axis }: { axis: AxisState }) {
  return (
    <div className="roberta-metric-strip">
      <div className="roberta-metric-strip__item">
        <span>Position</span>
        <strong>{axis.positionDeg.toFixed(2)} deg</strong>
      </div>
      <div className="roberta-metric-strip__item">
        <span>Velocity</span>
        <strong>{formatSigned(axis.velocityDegPerSec, "deg/s")}</strong>
      </div>
      <div className="roberta-metric-strip__item">
        <span>Drive current</span>
        <strong>{axis.currentAmps.toFixed(2)} A</strong>
      </div>
      <div className="roberta-metric-strip__item">
        <span>Bus voltage</span>
        <strong>{axis.busVoltage.toFixed(1)} V</strong>
      </div>
      <div className="roberta-metric-strip__item">
        <span>Temperature</span>
        <strong>{axis.temperatureC.toFixed(1)} C</strong>
      </div>
    </div>
  );
}

function AxisWorkspace({
  axis,
  jogStepDeg,
  onClearFault,
  onSetZero,
  onJog,
  onSetJogStep,
}: {
  axis: AxisState;
  jogStepDeg: number;
  onClearFault: () => void;
  onSetZero: () => void;
  onJog: (direction: "negative" | "positive") => void;
  onSetJogStep: (stepDeg: number) => void;
}) {
  return (
    <div className="roberta-axis-workspace">
      <div className="roberta-axis-workspace__heading">
        <p className="roberta-eyebrow">Axis workspace</p>
        <div className="roberta-axis-workspace__identity">
          <h2>{`${axis.shortLabel} · ${axis.name}`}</h2>
          <div className="roberta-axis-workspace__state-row">
            <span className={`roberta-chip roberta-chip--${axisStatusTone(axis)}`}>
              {axis.fault ?? axis.status}
            </span>
            <span className="roberta-axis-workspace__command">{axis.lastCommand}</span>
          </div>
        </div>
      </div>
      <div className="roberta-axis-workspace__hero">
        <div className="roberta-axis-workspace__readout">
          <div className="roberta-axis-workspace__position">
            {axis.positionDeg.toFixed(2)}
            <span>deg</span>
          </div>
          <div className="roberta-axis-workspace__supporting">
            <span>
              {axis.connected
                ? `Target ${axis.targetPositionDeg.toFixed(2)} deg`
                : "Controller disconnected"}
            </span>
            <span>{axis.controlMode} mode</span>
            <span>{axis.enabled ? "Drive enabled" : "Drive disabled"}</span>
            {axis.devicePath ? <span>{axis.devicePath}</span> : null}
          </div>
        </div>
        <div className="roberta-axis-workspace__status">
          <span className="roberta-field-label">Command state</span>
          <strong>{axis.lastCommand}</strong>
          <p>
            {!axis.connected
              ? "Connect the controller from workspace settings before issuing motion commands."
              : axis.fault
                ? "Operator action required before further motion."
                : "Axis ready for direct manipulation."}
          </p>
          <div className="roberta-axis-workspace__actions">
            {axis.connected ? (
              <Button onClick={onSetZero} size="sm" variant="outline">
                Set zero
              </Button>
            ) : null}
            {axis.fault ? (
              <Button onClick={onClearFault} size="sm" variant="outline">
                <RotateCcw className="h-4 w-4" />
                Reset fault
              </Button>
            ) : null}
          </div>
        </div>
      </div>
      <MetricStrip axis={axis} />
      <section className="roberta-control-cluster" aria-label="Jog controls">
        <div className="roberta-control-cluster__group">
          <span className="roberta-field-label">Step size</span>
          <div className="roberta-step-picker" role="group" aria-label="Jog step">
            {jogStepOptions.map((step) => (
              <button
                className={`roberta-step-button ${step === jogStepDeg ? "is-selected" : ""}`}
                key={step}
                onClick={() => onSetJogStep(step)}
                type="button"
              >
                {step.toFixed(step < 1 ? 1 : 0)} deg
              </button>
            ))}
          </div>
        </div>
        <div className="roberta-control-cluster__group roberta-control-cluster__group--motion">
          <span className="roberta-field-label">Motion</span>
          <div className="roberta-jogger__actions">
            <button
              aria-label={`Jog ${axis.shortLabel} negative`}
              className="roberta-motion-button roberta-motion-button--negative"
              disabled={!axis.connected}
              onClick={() => onJog("negative")}
              type="button"
            >
              <span className="roberta-motion-button__direction">Negative</span>
              <strong>-{jogStepDeg}</strong>
            </button>
            <div className="roberta-jogger__center">
              <span>{axis.shortLabel}</span>
              <strong>{axis.enabled ? "Ready" : "Disabled"}</strong>
            </div>
            <button
              aria-label={`Jog ${axis.shortLabel} positive`}
              className="roberta-motion-button roberta-motion-button--positive"
              disabled={!axis.connected}
              onClick={() => onJog("positive")}
              type="button"
            >
              <span className="roberta-motion-button__direction">Positive</span>
              <strong>+{jogStepDeg}</strong>
            </button>
          </div>
        </div>
      </section>
    </div>
  );
}

function RegisterGroupTabs({
  groups,
  selectedGroupId,
  onSelectGroup,
}: {
  groups: RegisterGroup[];
  selectedGroupId: string | null;
  onSelectGroup: (groupId: string) => void;
}) {
  return (
    <div className="roberta-register-tabs" role="tablist" aria-label="Register groups">
      {groups.map((group) => (
        <button
          className={`roberta-register-tab ${
            selectedGroupId === group.id ? "is-selected" : ""
          }`}
          key={group.id}
          onClick={() => onSelectGroup(group.id)}
          role="tab"
          type="button"
        >
          {group.label}
        </button>
      ))}
    </div>
  );
}

function RegisterList({
  activeGroupId,
  groups,
  selectedRegisterKey,
  onSelectRegister,
}: {
  activeGroupId: string | null;
  groups: RegisterGroup[];
  selectedRegisterKey: string | null;
  onSelectRegister: (groupId: string, registerKey: string) => void;
}) {
  return (
    <div className="roberta-register-groups">
      {groups
        .filter((group) => !activeGroupId || group.id === activeGroupId)
        .map((group) => (
          <section className="roberta-register-group" key={group.id}>
            <header className="roberta-register-group__header">
              <div>
                <h3>{group.label}</h3>
                <p>{group.description}</p>
              </div>
            </header>
            <div className="roberta-register-list">
              {group.registers.map((register) => (
                <button
                  className={`roberta-register-row ${
                    selectedRegisterKey === register.key ? "is-selected" : ""
                  } ${register.changed ? "is-changed" : ""}`}
                  key={register.key}
                  onClick={() => onSelectRegister(group.id, register.key)}
                  type="button"
                >
                  <span className="roberta-register-row__label">
                    {register.label}
                    <span className="roberta-register-row__description">
                      {register.description}
                    </span>
                  </span>
                  <span className="roberta-register-row__value">{register.value}</span>
                  <span
                    className={`roberta-register-row__access ${
                      register.writable ? "is-writable" : "is-readonly"
                    }`}
                  >
                    {register.writable ? "RW" : "RO"}
                  </span>
                </button>
              ))}
            </div>
          </section>
        ))}
    </div>
  );
}

function OverviewSurface({
  axes,
  selectedAxis,
  onSelectAxis,
}: {
  axes: AxisState[];
  selectedAxis: AxisState | null;
  onSelectAxis: (axisId: AxisState["id"]) => void;
}) {
  return (
    <div className="roberta-primary-surface">
      <SectionHeading
        eyebrow="Workspace overview"
        title="Robot state stays visible while you work"
        summary="Roberta is organized as a continuous workspace rather than a collection of pages."
      />
      <AxisMatrix
        axes={axes}
        onSelectAxis={onSelectAxis}
        selectedAxisId={selectedAxis?.id ?? axes[0]?.id ?? "axis-1"}
      />
      {selectedAxis ? (
        <section className="roberta-subsection">
          <div className="roberta-subsection__header">
            <h3>Current focus</h3>
            <p>
              The selected axis remains in view so you can shift between overview and direct
              work without losing operational context.
            </p>
          </div>
          <MetricStrip axis={selectedAxis} />
        </section>
      ) : null}
    </div>
  );
}

function AxesSurface({
  axes,
  jogStepDeg,
  onClearFault,
  onSetZero,
  onJog,
  onSelectAxis,
  onSetJogStep,
  selectedAxis,
}: {
  axes: AxisState[];
  jogStepDeg: number;
  onClearFault: () => void;
  onSetZero: () => void;
  onJog: (direction: "negative" | "positive") => void;
  onSelectAxis: (axisId: AxisState["id"]) => void;
  onSetJogStep: (stepDeg: number) => void;
  selectedAxis: AxisState | null;
}) {
  if (!selectedAxis) {
    return null;
  }

  return (
    <div className="roberta-axes-surface">
      <AxisRail
        axes={axes}
        onSelectAxis={onSelectAxis}
        selectedAxisId={selectedAxis.id}
      />
      <AxisWorkspace
        axis={selectedAxis}
        jogStepDeg={jogStepDeg}
        onClearFault={onClearFault}
        onSetZero={onSetZero}
        onJog={onJog}
        onSetJogStep={onSetJogStep}
      />
    </div>
  );
}

function RegistersSurface({
  groups,
  registerError,
  onSelectAxis,
  onSelectGroup,
  onSelectRegister,
  selectedAxis,
  selectedGroupId,
  selectedRegisterKey,
}: {
  groups: RegisterGroup[];
  registerError: string | null;
  onSelectAxis: (axisId: AxisState["id"]) => void;
  onSelectGroup: (groupId: string) => void;
  onSelectRegister: (groupId: string, registerKey: string) => void;
  selectedAxis: AxisState | null;
  selectedGroupId: string | null;
  selectedRegisterKey: string | null;
}) {
  if (!selectedAxis) {
    return null;
  }

  return (
    <div className="roberta-primary-surface">
      <SectionHeading
        eyebrow="Register inspection"
        title={`${selectedAxis.shortLabel} instrumentation`}
        summary="Register inspection stays dense and readable without turning into a generic spreadsheet."
      />
      <div className="roberta-register-toolbar">
        <button className="roberta-inline-action" onClick={() => onSelectAxis(selectedAxis.id)} type="button">
          {selectedAxis.shortLabel} selected
        </button>
        <span>{groups.length} groups</span>
        {registerError ? <span>{registerError}</span> : null}
      </div>
      <RegisterGroupTabs
        groups={groups}
        onSelectGroup={onSelectGroup}
        selectedGroupId={selectedGroupId}
      />
      <RegisterList
        activeGroupId={selectedGroupId}
        groups={groups}
        onSelectRegister={onSelectRegister}
        selectedRegisterKey={selectedRegisterKey}
      />
    </div>
  );
}

function DiagnosticsSurface({ events }: { events: DiagnosticsEvent[] }) {
  return (
    <div className="roberta-primary-surface">
      <SectionHeading
        eyebrow="Diagnostics"
        title="Transport and controller messages"
        summary="Diagnostics stay available as continuous context instead of hidden behind another screen."
      />
      <div className="roberta-event-list">
        {events.map((event) => (
          <div className="roberta-event-row" key={event.id}>
            <span className={`roberta-chip roberta-chip--${levelTone(event.level)}`}>
              {event.level}
            </span>
            <div className="roberta-event-row__body">
              <strong>{event.source}</strong>
              <p>{event.message}</p>
            </div>
            <span className="roberta-event-row__time">{event.timestampLabel}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

function ExperimentsSurface() {
  return (
    <div className="roberta-primary-surface">
      <SectionHeading
        eyebrow="Experiments"
        title="Workspace reserved for guided automation"
        summary="This surface will host scripted tasks, AI assistance, and controlled experiments without replacing direct manipulation."
      />
      <section className="roberta-subsection">
        <div className="roberta-subsection__header">
          <h3>Planned use</h3>
          <p>
            Experiment flows should operate on the same selected robot context that the axes
            and registers surfaces already expose.
          </p>
        </div>
        <ul className="roberta-plain-list">
          <li>Prompt-assisted motion rehearsal against mock or live hardware.</li>
          <li>Scripted register sweeps with immediate inspection feedback.</li>
          <li>Guided fault investigation without losing the active axis selection.</li>
        </ul>
      </section>
    </div>
  );
}

function InspectorSurface({
  activeSection,
  events,
  groups,
  selectedAxis,
  selectedRegister,
}: {
  activeSection: WorkspaceSection;
  events: DiagnosticsEvent[];
  groups: RegisterGroup[];
  selectedAxis: AxisState | null;
  selectedRegister: RegisterEntry | null;
}) {
  const previewRegisters = groups.flatMap((group) => group.registers).slice(0, 6);

  return (
    <aside className="roberta-inspector">
      <section className="roberta-inspector-section">
        <span className="roberta-field-label">Context</span>
        <h3>{sectionLabel(activeSection)}</h3>
        {selectedAxis ? (
          <div className="roberta-inspector-meta">
            <div>
              <span>Selected axis</span>
              <strong>
                {selectedAxis.shortLabel} · {selectedAxis.name}
              </strong>
            </div>
            <div>
              <span>State</span>
              <strong className={`roberta-text-${axisStatusTone(selectedAxis)}`}>
                {selectedAxis.fault ?? selectedAxis.status}
              </strong>
            </div>
            <div>
              <span>Last command</span>
              <strong>{selectedAxis.lastCommand}</strong>
            </div>
          </div>
        ) : null}
      </section>

      {activeSection === "registers" && selectedRegister ? (
        <section className="roberta-inspector-section">
          <span className="roberta-field-label">Selected register</span>
          <h3>{selectedRegister.label}</h3>
          <p className="roberta-secondary-copy">{selectedRegister.description}</p>
          <div className="roberta-inspector-meta">
            <div>
              <span>Value</span>
              <strong>{selectedRegister.value}</strong>
            </div>
            <div>
              <span>Raw</span>
              <strong>{selectedRegister.rawValue}</strong>
            </div>
            <div>
              <span>Access</span>
              <strong>{selectedRegister.writable ? "Writable" : "Read only"}</strong>
            </div>
            <div>
              <span>Changed this poll</span>
              <strong>{selectedRegister.changed ? "Yes" : "Stable"}</strong>
            </div>
          </div>
        </section>
      ) : (
        <section className="roberta-inspector-section">
          <span className="roberta-field-label">Register preview</span>
          <h3>Instrumentation watch</h3>
          <div className="roberta-inspector-list">
            {previewRegisters.map((register) => (
              <div className="roberta-inspector-list__row" key={register.key}>
                <span>{register.label}</span>
                <strong>{register.value}</strong>
              </div>
            ))}
          </div>
        </section>
      )}

      <section className="roberta-inspector-section">
        <span className="roberta-field-label">Recent events</span>
        <h3>Diagnostics stream</h3>
        <div className="roberta-inspector-events">
          {events.slice(0, 5).map((event) => (
            <div className="roberta-inspector-event" key={event.id}>
              <span className={`roberta-chip roberta-chip--${levelTone(event.level)}`}>
                {event.level}
              </span>
              <div>
                <strong>{event.source}</strong>
                <p>{event.message}</p>
              </div>
            </div>
          ))}
        </div>
      </section>
    </aside>
  );
}

function DiagnosticsDock({
  events,
  onToggle,
  open,
}: {
  events: DiagnosticsEvent[];
  onToggle: () => void;
  open: boolean;
}) {
  return (
    <section className={`roberta-diagnostics-dock ${open ? "is-open" : ""}`}>
      <button
        className="roberta-diagnostics-dock__toggle"
        onClick={onToggle}
        type="button"
      >
        <div>
          <span className="roberta-field-label">Diagnostics dock</span>
          <strong>{open ? "Expanded" : "Reserved for later expansion"}</strong>
        </div>
        <span>{open ? "Collapse" : `${events.length} recent events`}</span>
      </button>
      {open ? (
        <div className="roberta-diagnostics-dock__body">
          {events.slice(0, 6).map((event) => (
            <div className="roberta-event-row" key={event.id}>
              <span className={`roberta-chip roberta-chip--${levelTone(event.level)}`}>
                {event.level}
              </span>
              <div className="roberta-event-row__body">
                <strong>{event.source}</strong>
                <p>{event.message}</p>
              </div>
              <span className="roberta-event-row__time">{event.timestampLabel}</span>
            </div>
          ))}
        </div>
      ) : null}
    </section>
  );
}

export function App() {
  const {
    actions,
    recentDiagnostics,
    selectedAxis,
    selectedAxisRegisterGroups,
    selectedAxisRegisterError,
    selectedRegister,
    state,
  } = useRobertaWorkspace();

  const commandPaletteCommands = useMemo<CommandPaletteCommand[]>(() => {
    const axisCommands =
      state.snapshot?.axes.map((axis) => ({
        id: `select-${axis.id}`,
        label: `Select ${axis.shortLabel}`,
        description: `Focus ${axis.name} without leaving the current workspace.`,
        keywords: [axis.shortLabel, axis.name, "select axis"],
        action: () => actions.selectAxis(axis.id),
      })) ?? [];

    return [
      {
        id: "open-axes",
        label: "Open axis workspace",
        description: "Move the primary surface back to direct manipulation.",
        keywords: ["axes", "jog", "workspace"],
        action: () => actions.selectSection("axes"),
      },
      {
        id: "open-registers",
        label: "Open register inspection",
        description: "Bring dense register instrumentation into the primary surface.",
        keywords: ["registers", "inspection", "debug"],
        action: () => actions.selectSection("registers"),
      },
      {
        id: "open-diagnostics",
        label: "Open diagnostics",
        description: "Focus the workspace on controller and transport events.",
        keywords: ["diagnostics", "faults", "logs"],
        action: () => actions.selectSection("diagnostics"),
      },
      {
        id: "toggle-diagnostics-dock",
        label: "Toggle diagnostics dock",
        description: "Expand or collapse the reserved bottom diagnostics surface.",
        keywords: ["dock", "bottom panel", "diagnostics"],
        action: () => actions.toggleDiagnosticsDock(),
      },
      {
        id: "reset-selected-axis-fault",
        label: "Reset selected axis fault",
        description: "Clear the active axis fault if one is latched.",
        keywords: ["fault", "reset", "selected axis"],
        action: () => {
          void actions.clearSelectedAxisFault();
        },
      },
      ...axisCommands,
    ];
  }, [actions, state.snapshot?.axes]);

  useShellShortcuts([
    {
      id: "roberta.commandPalette",
      keys: ["k"],
      metaOrCtrl: true,
      description: "Open Roberta command palette",
      action: () => actions.setCommandPaletteOpen(true),
    },
  ]);

  const primarySurface = (() => {
    if (!state.snapshot) {
      return (
        <div className="roberta-primary-surface">
          <SectionHeading
            eyebrow="Workspace"
            title="Connecting desktop backend"
            summary="Waiting for the native host to deliver the first live workspace snapshot."
          />
        </div>
      );
    }

    switch (state.activeSection) {
      case "overview":
        return (
          <OverviewSurface
            axes={state.snapshot.axes}
            onSelectAxis={actions.selectAxis}
            selectedAxis={selectedAxis}
          />
        );
      case "axes":
        return (
          <AxesSurface
            axes={state.snapshot.axes}
            jogStepDeg={state.jogStepDeg}
            onClearFault={() => {
              void actions.clearSelectedAxisFault();
            }}
            onSetZero={() => {
              void actions.setSelectedAxisZero();
            }}
            onJog={(direction) => {
              void actions.jogSelectedAxis(direction);
            }}
            onSelectAxis={actions.selectAxis}
            onSetJogStep={actions.setJogStep}
            selectedAxis={selectedAxis}
          />
        );
      case "registers":
        return (
          <RegistersSurface
            groups={selectedAxisRegisterGroups}
            registerError={selectedAxisRegisterError}
            onSelectAxis={actions.selectAxis}
            onSelectGroup={(groupId) => {
              const group = selectedAxisRegisterGroups.find((entry) => entry.id === groupId);
              const firstRegister = group?.registers[0];

              if (firstRegister) {
                actions.selectRegister(groupId, firstRegister.key);
              }
            }}
            onSelectRegister={actions.selectRegister}
            selectedAxis={selectedAxis}
            selectedGroupId={state.selectedRegisterGroupId}
            selectedRegisterKey={state.selectedRegisterKey}
          />
        );
      case "diagnostics":
        return <DiagnosticsSurface events={recentDiagnostics} />;
      case "experiments":
        return <ExperimentsSurface />;
      default:
        return null;
    }
  })();

  const inspectorSurface = (
    <InspectorSurface
      activeSection={state.activeSection}
      events={recentDiagnostics}
      groups={selectedAxisRegisterGroups}
      selectedAxis={selectedAxis}
      selectedRegister={selectedRegister}
    />
  );

  return (
    <ThemeProvider defaultTheme="system" storageKey="roberta-theme">
      <AppShell
        header={
          <RobertaHeader
            activeSection={state.activeSection}
            connectionLabel={state.snapshot?.connection.label ?? "Connecting"}
            connectionMode={state.snapshot?.connection.mode ?? "disconnected"}
            modeLabel={state.snapshot?.modeLabel ?? "Loading"}
            onOpenCommandPalette={() => actions.setCommandPaletteOpen(true)}
            selectedAxis={selectedAxis}
          />
        }
        settingsPanel={
          <RobertaSettings
            availablePorts={state.snapshot?.availablePorts ?? []}
            axes={state.snapshot?.axes ?? []}
            axisConnectionDrafts={state.axisConnectionDrafts}
            connectionDetail={state.snapshot?.connection.detail ?? "Starting desktop backend"}
            diagnosticsDockOpen={state.diagnosticsDockOpen}
            modeLabel={state.snapshot?.modeLabel ?? "Loading"}
            onConnectAxis={(axisId) => {
              void actions.connectAxis(axisId);
            }}
            onDisconnectAxis={(axisId) => {
              void actions.disconnectAxis(axisId);
            }}
            onUpdateAxisConnectionDraft={actions.updateAxisConnectionDraft}
          />
        }
        sidebar={
          <RobertaSidebar
            activeSection={state.activeSection}
            onSelectSection={actions.selectSection}
          />
        }
        statusBar={
          <StatusBar
            leading={
              state.snapshot
                ? `${state.snapshot.connection.label} · ${state.snapshot.connection.pollHz} Hz · ${state.snapshot.connection.latencyMs} ms`
                : "Waiting for desktop backend"
            }
            trailing={
              <>
                <span>{selectedAxis ? `${selectedAxis.shortLabel} ${selectedAxis.name}` : "No axis selected"}</span>
                <span>{selectedAxis?.fault ?? selectedAxis?.lastCommand ?? "No active command"}</span>
              </>
            }
          />
        }
        storageNamespace="roberta"
      >
        <AppContent className="roberta-app-content">
          <div className="roberta-workspace-frame">
            <ShellPanelLayout
              className="roberta-workspace-panels"
              defaultLayout={[72, 28]}
              handleClassName="roberta-panel-handle"
              minSizes={[52, 20]}
              primary={primarySurface}
              primaryClassName="roberta-primary-panel"
              secondary={inspectorSurface}
              secondaryClassName="roberta-secondary-panel"
              storageId={`roberta/${state.activeSection}/panel-layout`}
            />
            <DiagnosticsDock
              events={recentDiagnostics}
              onToggle={actions.toggleDiagnosticsDock}
              open={state.diagnosticsDockOpen}
            />
          </div>
        </AppContent>
        <CommandPalette
          commands={commandPaletteCommands}
          onOpenChange={actions.setCommandPaletteOpen}
          onQueryChange={actions.setCommandQuery}
          open={state.commandPaletteOpen}
          query={state.commandQuery}
        />
      </AppShell>
    </ThemeProvider>
  );
}
