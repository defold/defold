#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Sync a GitHub Project V2 Score field from Size + Priority.

The script reads a project by node id, fetches the numeric custom fields
`Size`, `Priority`, and `Score`, computes `Size + Priority`, and writes the
result back to `Score`.

Examples:
  python3 scripts/update_github_project_score.py \
      --org defold \
      --project-id PVT_kwHO... \
      --dry-run

  GITHUB_TOKEN=ghp_... python3 scripts/update_github_project_score.py \
      --org defold \
      --project-id PVT_kwHO...
"""

import argparse
import os
import sys
from typing import Any, Dict, Optional

sys.path.append(os.path.join(os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')), "build_tools"))
import github


FIELD_NAMES = ("Size", "Priority", "Score")


def graphql(token, query, variables = None):
    response = github.query(query, token, variables = variables)
    if 'errors' in response:
        print(response)
        for error in response['errors']:
            print(error['message'])
        sys.exit(1)
    return response["data"]


def fetch_project(token: Optional[str], org: str, project_ref: str) -> Dict[str, Any]:
    query = """
    query($org: String!, $project_number: Int!, $after: String) {
      organization(login: $org) {
        projectV2(number: $project_number) {
          id
          title
          fields(first: 100, after: $after) {
            pageInfo {
              hasNextPage
              endCursor
            }
            nodes {
              __typename
              ... on ProjectV2FieldCommon {
                id
                name
              }
            }
          }
        }
      }
    }
    """

    project = None
    fields = {}
    after = None

    while True:
        data = graphql(
            token,
            query,
            {"org": org, "project_number": int(project_ref), "after": after},
        )
        node = ((data.get("organization") or {}).get("projectV2")) or {}
        if project is None:
            project = {
                "id": node.get("id"),
                "title": node.get("title"),
            }

        field_conn = node.get("fields") or {}
        for field in field_conn.get("nodes", []):
            field_id = field.get("id")
            field_name = field.get("name")
            if field_id and field_name:
                fields[field_name] = field_id

        if not field_conn.get("pageInfo", {}).get("hasNextPage"):
            break
        after = field_conn["pageInfo"]["endCursor"]

    if not project or not project.get("id"):
        raise RuntimeError("Could not load project %s" % project_ref)

    missing = [name for name in FIELD_NAMES if name not in fields]
    if missing:
        raise RuntimeError(
            "Project %s is missing required fields: %s"
            % (project["title"], ", ".join(missing))
        )

    project["fields"] = fields
    return project


def fetch_items(token: Optional[str], project_id: str) -> list:
    query = """
    query($project_id: ID!, $after: String) {
      node(id: $project_id) {
        ... on ProjectV2 {
          items(first: 100, after: $after) {
            pageInfo {
              hasNextPage
              endCursor
            }
            nodes {
              id
              content {
                __typename
                ... on Issue {
                  number
                  title
                  url
                }
                ... on PullRequest {
                  number
                  title
                  url
                }
                ... on DraftIssue {
                  title
                }
              }
              size: fieldValueByName(name: "Size") {
                __typename
                ... on ProjectV2ItemFieldSingleSelectValue {
                  name
                }
              }
              priority: fieldValueByName(name: "Priority") {
                __typename
                ... on ProjectV2ItemFieldSingleSelectValue {
                  name
                }
              }
              score: fieldValueByName(name: "Score") {
                __typename
                ... on ProjectV2ItemFieldNumberValue {
                  number
                }
              }
            }
          }
        }
      }
    }
    """

    items = []
    after = None
    while True:
        data = graphql(token, query, {"project_id": project_id, "after": after})
        conn = (data.get("node") or {}).get("items") or {}
        items.extend(conn.get("nodes", []))

        if not conn.get("pageInfo", {}).get("hasNextPage"):
            break
        after = conn["pageInfo"]["endCursor"]

    return items


def number_or_zero(field_value: Optional[Dict[str, Any]]) -> float:
    if field_value:
        if field_value.get("__typename") == "ProjectV2ItemFieldSingleSelectValue":
            return int(field_value.get("name"))
        if field_value.get("__typename") == "ProjectV2ItemFieldNumberValue":
            return field_value.get("number")
    return 0


def item_label(item: Dict[str, Any]) -> str:
    content = item.get("content") or {}
    ctype = content.get("__typename")
    if ctype in ("Issue", "PullRequest"):
        number = content.get("number")
        title = content.get("title") or ""
        return "#%s %s" % (number, title) if number is not None else title
    if ctype == "DraftIssue":
        return content.get("title") or item.get("id") or "draft issue"
    return item.get("id") or "unknown item"


def update_score(token: Optional[str], project_id: str, item_id: str, field_id: str, score: Any) -> None:
    mutation = """
    mutation($project_id: ID!, $item_id: ID!, $field_id: ID!, $value: ProjectV2FieldValue!) {
      updateProjectV2ItemFieldValue(
        input: {
          projectId: $project_id
          itemId: $item_id
          fieldId: $field_id
          value: $value
        }
      ) {
        projectV2Item {
          id
        }
      }
    }
    """

    graphql(
        token,
        mutation,
        {
            "project_id": project_id,
            "item_id": item_id,
            "field_id": field_id,
            "value": {"number": score},
        },
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Sync Score = Size + Priority for a GitHub Project V2.")
    parser.add_argument("--org", required=True, help="GitHub organization login")
    parser.add_argument("--project-id", required=True, help="GitHub Project V2 node id or numeric project number")
    parser.add_argument("--token", help="GitHub token. Defaults to GITHUB_TOKEN or GH_TOKEN.")
    parser.add_argument("--dry-run", action="store_true", help="Print calculated updates without writing them")
    args = parser.parse_args()

    token = args.token or os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")

    project = fetch_project(token, args.org, args.project_id)
    items = fetch_items(token, project["id"])

    size_field_id = project["fields"]["Size"]
    priority_field_id = project["fields"]["Priority"]
    score_field_id = project["fields"]["Score"]

    changed = 0
    skipped = 0

    print("Project: %s (%s) in %s" % (project["title"], project["id"], args.org))
    for item in items:
        label = item_label(item)
        size = number_or_zero(item.get("size"))
        priority = number_or_zero(item.get("priority"))
        current_score = number_or_zero(item.get("score"))
        score = round((priority * priority) / size, 2) if size > 0 else 0

        if current_score == score:
            skipped += 1
            print("unchanged: %s -> Score: %s (Priority: %s / Size: %s)" % (label, score, priority, size))
            continue

        changed += 1
        print("update: %s -> Score: %s (Priority: %s / Size: %s)" % (label, score, priority, size))
        if not args.dry_run:
            update_score(token, project["id"], item["id"], score_field_id, score)

    print("Processed %d items, %d updated, %d unchanged" % (len(items), changed, skipped))
    if args.dry_run:
        print("Dry run only; no changes were written.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
