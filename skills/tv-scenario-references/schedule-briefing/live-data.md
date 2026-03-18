# Live Data Notes

- Verified live ICS feed access on March 15, 2026 using:
  `https://holidays.hyunbin.page/basic.ics`
- The skill can emit A2UI directly from a live ICS URL with:
  `python3 scripts/generate_schedule_a2ui.py --source ics-url --ics-url https://holidays.hyunbin.page/basic.ics`
- The same adapter also supports local ICS files with:
  `python3 scripts/generate_schedule_a2ui.py --source ics-file --ics-file /path/to/calendar.ics`
- The current adapter expands RRULE-based recurring events inside the selected window, but privacy-safe summarization still needs to be enforced by the caller.
