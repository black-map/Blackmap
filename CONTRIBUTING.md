# Contributing to BlackMap

Thank you for your interest in making **BlackMap** a better framework! We welcome all contributions, big or small. 

## Code of Conduct

Please treat all maintainers and contributors with respect. We want to foster a healthy open source environment.

## Workflow

1. **Fork the repo** and create a feature branch (`git checkout -b feature/my-new-feature`).
2. Make your modifications.
    * Ensure your code passes all Rust syntax rules by running `cargo fmt`.
    * Ensure no warnings are triggered by running `cargo clippy`.
3. Add **Unit Tests** for any new features in `rust/src/` or integration tests in `/tests`.
4. **Push** your branch and open a Pull Request against the `main` branch.

## Issue Templates

We recommend following these templates when creating issues on GitHub:

### Bug Report:
```
**Describe the bug:**
**Version used:** (e.g. `blackmap --version`)
**Command executed:**
**OS:**
```

### Feature Request:
```
**Description of Feature:**
**Why is this needed:**
```

## Plugin Submissions

If you write a Rust dynamically loaded plugin (`.so`), please submit it as an independent repository, and we will feature it in our forthcoming `PLUGINS.md` marketplace directory instead of merging it into the core project tree.
