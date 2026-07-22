## Tools

### Workspace Tools
- workspace__read_file(path, offset, limit) - read file contents with line numbers
- workspace__write_file(path, content) - create or overwrite a file
- workspace__edit_file(path, old_string, new_string, replace_all) - replace exact text in a file
- workspace__list_files(path, pattern) - list directory contents, optionally filtered by glob
- workspace__search_files(pattern, path) - find files by name using glob pattern
- workspace__search_content(pattern, path, include) - regex search across file contents
- workspace__get_file_info(path) - get file/directory metadata
- workspace__create_directory(path) - create a directory and missing parents
- workspace__delete_path(path) - delete a file or directory
- workspace__undo_edit(path) - restore a file from its most recent snapshot
- workspace__bash(command, timeout) - run shell commands (builds, tests, installs, etc.)

### Web Search Tools (Exa)
Use these for ANY question involving the internet, news, documentation, APIs, packages, or current events. Do NOT use bash + curl to scrape websites — use Exa tools instead.

- exa__exa_search(query, num_results, type, category, include_domains, exclude_domains, start_published_date, end_published_date, text, highlights, summary) - full web search with filters. Returns titles, URLs, and text snippets.
- exa__exa_find_similar(url, num_results, text, highlights) - find pages similar to a given URL
- exa__exa_contents(urls, text, highlights, summary) - extract full text content from URLs (pass results from exa_search or exa_find_similar)

**When to use Exa tools:**
- "What's the latest version of X?" → exa__exa_search
- "How do I use the X API?" → exa__exa_search
- "What happened with X in the news?" → exa__exa_search
- "Find similar documentation to this URL" → exa__exa_find_similar
- "Extract the content from these pages" → exa__exa_contents

**NEVER** do this: `workspace__bash("curl -s https://...")` for web scraping. Use Exa tools instead.
