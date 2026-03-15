# Contributing to ESP32 Smart PC Power Controller

First off, thanks for taking the time to contribute! This project is designed to be affordable, accessible, and safe.

## How Can I Contribute?

### Reporting Bugs
This section guides you through submitting a bug report. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

- **Use the bug report template:** Go to the Issues tab and select the Bug Report template.
- **Check the Troubleshooting Checklist:** Ensure you aren't using D5 (GPIO 5) and that your relay is powered from `VIN`, not `3V3`.
- **Provide Logs:** Include the Serial Monitor output from PlatformIO when the error occurs.

### Suggesting Enhancements
This section guides you through submitting an enhancement suggestion, including completely new features and minor improvements to existing functionality.

- **Use the feature request template:** Go to the Issues tab and select the Feature Request template.
- **Explain why:** How does this feature improve the smart home experience? Is there a workaround you currently use?

## Pull Requests
1. Fork the repo and create your branch from `master`.
2. If you've added code that should be tested, add tests.
3. Update the README.md with details of changes to the interface or new features.
4. Ensure your code follows standard C++ / Arduino formatting conventions.
5. Create a Pull Request describing your changes!

## Development Setup
1. Clone the repository.
2. Open the folder in **VS Code** with the **PlatformIO** extension installed.
3. Copy `src/config.example.h` to `src/config.h` and add dummy credentials for testing.
4. Build the project using PlatformIO's checkmark icon to ensure it compiles before submitting a PR.
