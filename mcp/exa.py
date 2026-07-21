import os
import requests
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Exa")

BASE_URL = "https://ai.hackclub.com/proxy/v1/exa"


def _get_api_key() -> str:
    key = os.environ.get("EXA_KEY", "")
    if not key:
        raise RuntimeError("EXA_key environment variable not set")
    return key


def _headers() -> dict:
    return {
        "Authorization": f"Bearer {_get_api_key()}",
        "Content-Type": "application/json",
    }


def _post(endpoint: str, body: dict) -> dict:
    resp = requests.post(
        f"{BASE_URL}/{endpoint}",
        headers=_headers(),
        json=body,
        timeout=30,
    )
    if resp.status_code != 200:
        raise RuntimeError(
            f"Exa API error ({resp.status_code}): {resp.text[:500]}"
        )
    return resp.json()


def _format_result(result: dict, index: int) -> str:
    parts = [f"[{index + 1}] {result.get('title', 'Untitled')}"]
    parts.append(f"    URL: {result.get('url', '')}")

    if result.get("publishedDate"):
        parts.append(f"    Published: {result['publishedDate']}")
    if result.get("author"):
        parts.append(f"    Author: {result['author']}")

    if result.get("highlights"):
        for h in result["highlights"]:
            parts.append(f"    Highlight: {h}")

    if result.get("summary"):
        parts.append(f"    Summary: {result['summary']}")

    if result.get("text"):
        text = result["text"]
        if len(text) > 1500:
            text = text[:1500] + f"... [truncated, {len(result['text'])} chars total]"
        parts.append(f"    Text: {text}")

    return "\n".join(parts)


@mcp.tool()
def exa_search(
    query: str,
    num_results: int = 5,
    type: str = "",
    category: str = "",
    include_domains: list[str] = None,
    exclude_domains: list[str] = None,
    start_published_date: str = "",
    end_published_date: str = "",
    text: bool = False,
    highlights: bool = False,
    summary: str = "",
) -> str:
    """
    Search the web using Exa. Returns relevant results with titles, URLs,
    and optionally text content, highlights, or summaries.

    Args:
        query: The search query string
        num_results: Number of results to return (default 5, max 100)
        type: Search mode - auto, fast, instant, deep-lite, deep, deep-reasoning (default auto)
        category: Focus category - company, research paper, news, financial report, people, personal site, publication
        include_domains: Only return results from these domains (e.g. ["arxiv.org", "github.com"])
        exclude_domains: Exclude results from these domains
        start_published_date: Only results published after this date (ISO 8601, e.g. "2024-01-01T00:00:00.000Z")
        end_published_date: Only results published before this date (ISO 8601)
        text: If true, include full page text in results (default false)
        highlights: If true, include LLM-selected highlights from each page (default false)
        summary: Custom query for generating summaries of each result (e.g. "Main findings")
    """
    body = {"query": query, "numResults": min(num_results, 100)}

    if type:
        body["type"] = type
    if category:
        body["category"] = category
    if include_domains:
        body["includeDomains"] = include_domains
    if exclude_domains:
        body["excludeDomains"] = exclude_domains
    if start_published_date:
        body["startPublishedDate"] = start_published_date
    if end_published_date:
        body["endPublishedDate"] = end_published_date

    contents = {}
    if text:
        contents["text"] = True
    if highlights:
        contents["highlights"] = True
    if summary:
        contents["summary"] = {"query": summary}
    if contents:
        body["contents"] = contents

    data = _post("search", body)
    results = data.get("results", [])

    if not results:
        return "(no results found)"

    output = []
    for i, result in enumerate(results):
        output.append(_format_result(result, i))

    cost = data.get("costDollars", {}).get("total", "unknown")
    output.append(f"\n({len(results)} results, cost: ${cost})")

    return "\n\n".join(output)


@mcp.tool()
def exa_find_similar(
    url: str,
    num_results: int = 5,
    text: bool = False,
    highlights: bool = False,
) -> str:
    """
    Find pages similar to a given URL using Exa.

    Args:
        url: The URL to find similar pages for
        num_results: Number of results to return (default 5, max 100)
        text: If true, include full page text in results (default false)
        highlights: If true, include LLM-selected highlights from each page (default false)
    """
    body = {"url": url, "numResults": min(num_results, 100)}

    contents = {}
    if text:
        contents["text"] = True
    if highlights:
        contents["highlights"] = True
    if contents:
        body["contents"] = contents

    data = _post("findSimilar", body)
    results = data.get("results", [])

    if not results:
        return "(no similar pages found)"

    output = []
    for i, result in enumerate(results):
        output.append(_format_result(result, i))

    cost = data.get("costDollars", {}).get("total", "unknown")
    output.append(f"\n({len(results)} results, cost: ${cost})")

    return "\n\n".join(output)


@mcp.tool()
def exa_contents(
    urls: list[str],
    text: bool = True,
    highlights: bool = False,
    summary: str = "",
) -> str:
    """
    Extract page contents from one or more URLs using Exa.

    Args:
        urls: List of URLs to extract content from
        text: If true, return full page text (default true)
        highlights: If true, include LLM-selected highlights (default false)
        summary: Custom query for generating summaries of each page (e.g. "Key points")
    """
    body = {"urls": urls}

    contents = {}
    if text:
        contents["text"] = True
    if highlights:
        contents["highlights"] = True
    if summary:
        contents["summary"] = {"query": summary}
    if contents:
        body["contents"] = contents

    data = _post("contents", body)
    results = data.get("results", [])

    if not results:
        return "(no content extracted)"

    output = []
    for i, result in enumerate(results):
        output.append(_format_result(result, i))

    cost = data.get("costDollars", {}).get("total", "unknown")
    output.append(f"\n({len(results)} pages, cost: ${cost})")

    return "\n\n".join(output)


if __name__ == "__main__":
    mcp.run()
