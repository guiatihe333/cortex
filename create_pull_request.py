# Simple script to create a pull request via GitHub API using PyGithub
# Requires GITHUB_TOKEN environment variable or --token argument

import os
import argparse
from github import Github


def parse_args():
    parser = argparse.ArgumentParser(description="Create a GitHub pull request")
    parser.add_argument("repository", help="Repository in the form 'owner/repo'")
    parser.add_argument("head", help="Branch with changes, e.g. 'feature-branch'")
    parser.add_argument("base", help="Branch you want to merge into, e.g. 'main'")
    parser.add_argument("title", help="Title of the pull request")
    parser.add_argument("body", help="Body text for the pull request")
    parser.add_argument("--token", help="GitHub access token. Can also use GITHUB_TOKEN env var")
    return parser.parse_args()


def main():
    args = parse_args()
    token = args.token or os.environ.get("GITHUB_TOKEN")
    if not token:
        raise SystemExit("GitHub token required via --token or GITHUB_TOKEN env var")

    gh = Github(token)
    repo = gh.get_repo(args.repository)
    pr = repo.create_pull(
        title=args.title,
        body=args.body,
        head=args.head,
        base=args.base,
    )
    print(f"Pull request created: {pr.html_url}")


if __name__ == "__main__":
    main()

