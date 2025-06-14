# lostgear

## Python Version Compatibility

This project depends on `python-telegram-bot==13.15`, which officially
supports Python versions 3.8 through 3.11. Running the bot with
Python 3.12 may cause unexpected issues, so it is recommended to
stick to Python 3.8–3.11 for now.

## Generating `project.zip` and `README.pdf`

The repository does not store the ZIP archive or PDF. If you need them,
you can create them locally using the following commands:

```bash
zip -r project.zip .
pandoc README.md -o README.pdf
```

## Creating a Pull Request from Python

A helper script `create_pull_request.py` is included to open a pull request
using the GitHub API via the `PyGithub` library.

First install the dependency:

```bash
pip install PyGithub
```

Then run the script, providing the repository, branches, title, and body.
A personal access token can be supplied with the `--token` option or via the
`GITHUB_TOKEN` environment variable.

```bash
python create_pull_request.py owner/repo feature-branch main "My PR" "Description" --token YOUR_TOKEN
```
