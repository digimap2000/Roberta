import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { beforeEach, describe, expect, it, vi } from "vitest";

vi.mock("./services/live-robot-service", async () => {
  const { createFakeRobotService } = await import("./test/fake-robot-service");

  return {
    createLiveRobotService: createFakeRobotService,
  };
});

import { App } from "./App";

describe("Roberta workspace", () => {
  beforeEach(() => {
    window.localStorage.clear();
    document.documentElement.className = "";
    document.documentElement.removeAttribute("data-theme");
    document.documentElement.removeAttribute("data-shell-density");
    document.documentElement.style.removeProperty("--app-shell-font-scale");
  });

  it("renders the operator workspace with all six axes visible", async () => {
    render(<App />);

    await screen.findByRole("button", { name: "Jog A1 positive" });
    expect(screen.getByRole("heading", { name: "Roberta" })).toBeInTheDocument();
    expect(
      screen.getByRole("heading", { level: 2, name: /A1 · Base/i }),
    ).toBeInTheDocument();
    expect(screen.getAllByRole("button", { name: /A1/i }).length).toBeGreaterThan(0);
    expect(screen.getAllByRole("button", { name: /A2/i }).length).toBeGreaterThan(0);
    expect(screen.getAllByRole("button", { name: /A3/i }).length).toBeGreaterThan(0);
    expect(screen.getAllByRole("button", { name: /A4/i }).length).toBeGreaterThan(0);
    expect(screen.getAllByRole("button", { name: /A5/i }).length).toBeGreaterThan(0);
    expect(screen.getAllByRole("button", { name: /A6/i }).length).toBeGreaterThan(0);
    expect(screen.getByRole("group", { name: "Jog step" })).toBeInTheDocument();
  });

  it("updates the workspace in place when selecting and jogging an axis", async () => {
    const user = userEvent.setup();

    render(<App />);

    await screen.findByRole("button", { name: "Jog A1 positive" });
    await user.click(screen.getAllByRole("button", { name: /A2/i })[0]!);
    expect(
      screen.getByRole("heading", { level: 2, name: /A2 · Shoulder/i }),
    ).toBeInTheDocument();

    await user.click(screen.getByRole("button", { name: "Jog A2 positive" }));

    await waitFor(() =>
      expect(screen.getAllByText("Step jog +1.0 deg").length).toBeGreaterThan(0),
    );
  });

  it("opens register inspection without losing the selected axis context", async () => {
    const user = userEvent.setup();

    render(<App />);

    await screen.findByRole("button", { name: "Jog A1 positive" });
    await user.click(screen.getByRole("button", { name: /Registers/i }));
    expect(
      screen.getByRole("heading", { name: /A1 instrumentation/i }),
    ).toBeInTheDocument();

    await user.click(screen.getByRole("tab", { name: /Drive/i }));
    await user.click(screen.getByRole("button", { name: /Control Word/i }));

    expect(screen.getByRole("heading", { name: "Control Word" })).toBeInTheDocument();
    expect(screen.getByText("Writable")).toBeInTheDocument();
  });

  it("keeps shell and workspace shortcuts active", async () => {
    const user = userEvent.setup();

    render(<App />);

    await screen.findByRole("button", { name: "Jog A1 positive" });
    await user.keyboard("{Control>}b{/Control}");
    await waitFor(() =>
      expect(window.localStorage.getItem("roberta:sidebar-collapsed")).toBe("true"),
    );

    await user.keyboard("{Control>}k{/Control}");
    expect(screen.getByRole("heading", { name: "Command palette" })).toBeInTheDocument();

    await user.click(screen.getByRole("button", { name: /Open diagnostics/i }));
    expect(
      screen.getByRole("heading", { name: "Transport and controller messages" }),
    ).toBeInTheDocument();
  });
});
