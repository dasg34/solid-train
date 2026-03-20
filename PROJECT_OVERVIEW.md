# Tizen Tool TV Project Overview

## What This Project Is

This workspace is a Samsung Tizen TV presentation project for turning natural
language requests into TV-friendly viewer surfaces.

The project is designed around a simple split:

- domain data is fetched and normalized outside the viewer
- a presentation JSON payload is composed from that data
- the TV viewer app receives that payload and renders a deterministic UI

The goal is not to show a chat transcript on TV. The goal is to show a large,
clear, glanceable surface that feels native to a TV context.

## Core Flow

```text
User request
  -> normalized domain data
  -> presentation JSON
  -> runtime handoff
  -> Tizen TV viewer
  -> deterministic TV UI
```

In practice, the current pipeline is:

```text
user request
  -> tizen-tool-domain-fetch
  -> presentation composition
  -> tizen-tool-viewer-launch
  -> tizen-tool-viewer
```

## Main Projects In This Workspace

### `tizen-tool-viewer`

Flutter-based Tizen TV viewer app.

- receives presentation JSON
- converts it into deterministic internal UI messages
- renders TV surfaces with `genui`
- applies TV-oriented layout, typography, and shell treatment

This is the visible product surface.

### `tizen-tool-domain-fetch`

Native CLI for fetching and normalizing domain data.

- weather
- news
- finance
- commute
- sports
- schedule
- travel
- emergency
- daily

This project stops at normalized domain JSON. It does not render UI.

### `tizen-tool-viewer-launch`

Native CLI for sending presentation JSON to the TV viewer at runtime.

- accepts payload from `stdin` or `--file`
- sends a Tizen App Control launch request
- targets `org.tizen.tizen-tool-viewer`

This is the runtime handoff tool.

### `tizen-tool-presentation-validate`

Native validator for presentation JSON payloads.

- checks required fields
- checks payload structure
- catches invalid presentation documents before handoff

### `skills/`

Agent-facing instructions for composing and launching payloads.

Important skills in this workspace include:

- `tizen-tool-presentation-catalog`
- `tizen-tool-viewer-launcher`
- `tizen-tool-domain-fetch`

These define how an AI agent should fetch data, shape presentation JSON, and
send it to the viewer.

### `genui/`

Upstream reference repository used for rendering internals, examples, and
catalog behavior.

It is treated as an upstream dependency and reference codebase, not as the main
product app.

## Presentation Model

The viewer does not expect a low-level layout tree from the LLM.

Instead, the system uses a presentation-first contract. A payload describes
meaningful content such as:

- title
- summary
- hero
- metrics
- facts
- chart
- alert

The viewer then decides how to render those pieces in a stable TV layout.

This keeps the system more robust than asking an LLM to directly generate
low-level UI structure every time.

## Why This Architecture

This project is intentionally split to keep responsibilities clean.

- `tizen-tool-domain-fetch` handles data access and normalization
- presentation composition handles semantic shaping
- `tizen-tool-viewer` handles rendering only

That separation gives us:

- predictable TV layouts
- easier validation
- safer handoff boundaries
- faster iteration on visual presentation

## Target Experience

The project is optimized for a Korean TV environment.

- primary locale: `ko-KR`
- primary timezone: `Asia/Seoul`
- 10-foot readability
- simple remote-friendly interaction
- glanceable information hierarchy

Typical surfaces include:

- weather
- commute
- finance snapshot
- news
- daily briefing
- schedule
- emergency mode

## Current Direction

The current direction is presentation-first runtime rendering.

That means:

- the payload format is presentation JSON
- the viewer is the renderer
- runtime handoff is done through the launcher
- validation is handled before handoff

The system is no longer centered on externally authored low-level UI payloads.

## Quick Summary

If you only remember one thing, remember this:

```text
This workspace builds a Tizen TV viewer system that turns normalized domain
data into validated presentation payloads and renders them as deterministic,
TV-friendly surfaces.
```
