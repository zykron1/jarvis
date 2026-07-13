from mcp.server.fastmcp import FastMCP
from datetime import datetime
import uuid

mcp = FastMCP("Meeting Server")

meetings = [
    {
        "id": "1",
        "title": "Project Kickoff",
        "time": "2026-07-13 10:00",
        "participants": ["alice@example.com", "bob@example.com"]
    },
    {
        "id": "2",
        "title": "Design Review",
        "time": "2026-07-14 14:30",
        "participants": ["charlie@example.com"]
    }
]


def find_meeting(meeting_id: str) -> dict | None:
    for meeting in meetings:
        if meeting["id"] == meeting_id:
            return meeting
    return None


@mcp.tool()
def list_meetings() -> list[dict]:
    """
    Returns all scheduled meetings.
    """
    return meetings


@mcp.tool()
def get_meeting(meeting_id: str) -> dict:
    """
    Returns a single meeting by id.

    Args:
        meeting_id: ID of the meeting to fetch
    """
    meeting = find_meeting(meeting_id)
    if not meeting:
        raise ValueError(f"No meeting found with id {meeting_id}")
    return meeting


@mcp.tool()
def create_meeting(
    title: str,
    time: str,
    participants: list[str]
) -> dict:
    """
    Creates a new meeting.

    Args:
        title: Meeting title
        time: Meeting time string
        participants: List of participant emails
    """
    meeting = {
        "id": str(uuid.uuid4()),
        "title": title,
        "time": time,
        "participants": participants,
        "created_at": datetime.now().isoformat()
    }
    meetings.append(meeting)
    return meeting


@mcp.tool()
def update_meeting(
    meeting_id: str,
    title: str | None = None,
    time: str | None = None,
    participants: list[str] | None = None
) -> dict:
    """
    Updates fields on an existing meeting.

    Args:
        meeting_id: ID of the meeting to update
        title: New title, if changing
        time: New time, if changing
        participants: New participant list, if changing
    """
    meeting = find_meeting(meeting_id)
    if not meeting:
        raise ValueError(f"No meeting found with id {meeting_id}")

    if title is not None:
        meeting["title"] = title
    if time is not None:
        meeting["time"] = time
    if participants is not None:
        meeting["participants"] = participants

    meeting["updated_at"] = datetime.now().isoformat()
    return meeting


@mcp.tool()
def delete_meeting(meeting_id: str) -> dict:
    """
    Deletes a meeting by id.

    Args:
        meeting_id: ID of the meeting to delete
    """
    meeting = find_meeting(meeting_id)
    if not meeting:
        raise ValueError(f"No meeting found with id {meeting_id}")
    meetings.remove(meeting)
    return {"deleted": True, "id": meeting_id}


@mcp.tool()
def add_participant(meeting_id: str, email: str) -> dict:
    """
    Adds a participant to a meeting.

    Args:
        meeting_id: ID of the meeting
        email: Participant email to add
    """
    meeting = find_meeting(meeting_id)
    if not meeting:
        raise ValueError(f"No meeting found with id {meeting_id}")
    if email not in meeting["participants"]:
        meeting["participants"].append(email)
    return meeting


@mcp.tool()
def remove_participant(meeting_id: str, email: str) -> dict:
    """
    Removes a participant from a meeting.

    Args:
        meeting_id: ID of the meeting
        email: Participant email to remove
    """
    meeting = find_meeting(meeting_id)
    if not meeting:
        raise ValueError(f"No meeting found with id {meeting_id}")
    if email in meeting["participants"]:
        meeting["participants"].remove(email)
    return meeting


@mcp.tool()
def find_meetings_by_participant(email: str) -> list[dict]:
    """
    Returns all meetings that include a given participant.

    Args:
        email: Participant email to search for
    """
    return [m for m in meetings if email in m["participants"]]


@mcp.tool()
def find_meetings_in_range(start: str, end: str) -> list[dict]:
    """
    Returns meetings whose time falls within a given range.

    Args:
        start: Start time string, format YYYY-MM-DD HH:MM
        end: End time string, format YYYY-MM-DD HH:MM
    """
    start_dt = datetime.fromisoformat(start)
    end_dt = datetime.fromisoformat(end)
    result = []
    for m in meetings:
        m_dt = datetime.fromisoformat(m["time"])
        if start_dt <= m_dt <= end_dt:
            result.append(m)
    return result


@mcp.tool()
def check_conflict(time: str, participants: list[str]) -> dict:
    """
    Checks whether any given participants already have a meeting at the given time.

    Args:
        time: Proposed meeting time string
        participants: List of participant emails to check
    """
    conflicts = []
    for m in meetings:
        if m["time"] == time:
            overlap = set(m["participants"]) & set(participants)
            if overlap:
                conflicts.append({"meeting_id": m["id"], "title": m["title"], "overlapping": list(overlap)})
    return {"has_conflict": len(conflicts) > 0, "conflicts": conflicts}


if __name__ == "__main__":
    mcp.run()
