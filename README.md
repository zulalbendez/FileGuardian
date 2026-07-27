# FileGuardian

FileGuardian is a C++17 based file scanning and sensitive data detection application.

FileGuardian is a C++17 project that scans files, detects sensitive keywords, creates tags, and stores results using SQLite.

## Features

- Recursive directory scanning
- File metadata extraction
- Sensitive data detection
- Keyword-based analysis
- Risk level classification
- Automatic file tagging
- SQLite database storage

## Technologies

- C++17
- SQLite3
- STL
- std::filesystem

## Risk Classification

| Level | Description |
|---|---|
| LOW | Password related keywords |
| MEDIUM | Confidential information |
| HIGH | Identity numbers, API keys, credit card keywords |

## Database Structure

The application uses SQLite database with three main tables:

### files

Stores scanned file information:

- Filename
- File path
- Extension
- Size
- Modified date
- Risk level
- Detected keywords

### tags

Stores generated tags for files.

### file_tags

Creates the relationship between files and tags.

## Usage

Run the application with a directory path.

Example:

FileGuardian C:\Users\User\Documents


## Project Structure

```
FileGuardian
│
├── main.cpp
├── sqlite3.c
├── sqlite3.h
├── README.md
└── .gitignore
```

## Future Improvements

- Regex based sensitive data detection
- SHA-256 file hashing
- Duplicate file detection
- JSON export
- GUI interface
- OCR support
- Machine learning based classification

## License

MIT License