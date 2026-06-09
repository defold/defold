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

"""Copy a GitHub Project V2 custom field value to another project.

The script reads all issues in a source project, gets the value of a named
custom field, finds the same issue in a target project, and writes the value to
a named target custom field.

Examples:
  python3 scripts/copy_github_project_field.py \
      --org defold \
      --source-project-id 12 \
      --target-project-id 34 \
      --source-field "Effort" \
      --target-field "Imported Effort" \
      --dry-run

  GITHUB_TOKEN=ghp_... python3 scripts/copy_github_project_field.py \
      --org defold \
      --source-project-id PVT_kwHO... \
      --target-project-id PVT_kwHO... \
      --source-field "Size" \
      --target-field "Estimate"
"""

import argparse
import os
import sys
from typing import Any, Dict, Optional

sys.path.append(os.path.join(os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')), "build_tools"))
import github


FIELD_TYPE_NAMES = (
    "ProjectV2Field",
    "ProjectV2SingleSelectField",
    "ProjectV2IterationField",
)


def graphql(token, query, variables = None):
    response = github.query(query, token, variables = variables)
    if not response:
        print("No response from GitHub GraphQL API")
        sys.exit(1)
    if 'errors' in response:
        print(response)
        for error in response['errors']:
            print(error['message'])
        sys.exit(1)
    return response["data"]


def fetch_project_by_number(token: Optional[str], org: str, project_number: int) -> Dict[str, Any]:
    query = """
    query($org: String!, $project_number: Int!) {
      organization(login: $org) {
        projectV2(number: $project_number) {
          id
          title
          number
        }
      }
    }
    """

    data = graphql(token, query, {"org": org, "project_number": project_number})
    project = ((data.get("organization") or {}).get("projectV2")) or {}
    if not project.get("id"):
        raise RuntimeError("Could not load project number %s in %s" % (project_number, org))
    return project


def fetch_project_by_id(token: Optional[str], project_id: str) -> Dict[str, Any]:
    query = """
    query($project_id: ID!) {
      node(id: $project_id) {
        ... on ProjectV2 {
          id
          title
          number
        }
      }
    }
    """

    data = graphql(token, query, {"project_id": project_id})
    project = data.get("node") or {}
    if not project.get("id"):
        raise RuntimeError("Could not load project id %s" % project_id)
    return project


def fetch_project(token: Optional[str], org: str, project_ref: str) -> Dict[str, Any]:
    if project_ref.isdigit():
        return fetch_project_by_number(token, org, int(project_ref))
    return fetch_project_by_id(token, project_ref)


def fetch_project_fields(token: Optional[str], project_id: str) -> Dict[str, Any]:
    query = """
    query($project_id: ID!, $after: String) {
      node(id: $project_id) {
        ... on ProjectV2 {
          fields(first: 100, after: $after) {
            pageInfo {
              hasNextPage
              endCursor
            }
            nodes {
              __typename
              ... on ProjectV2Field {
                id
                name
                dataType
              }
              ... on ProjectV2SingleSelectField {
                id
                name
                dataType
                options {
                  id
                  name
                }
              }
              ... on ProjectV2IterationField {
                id
                name
                dataType
                configuration {
                  iterations {
                    id
                    title
                  }
                }
              }
            }
          }
        }
      }
    }
    """

    fields = {}
    after = None
    while True:
        data = graphql(token, query, {"project_id": project_id, "after": after})
        conn = ((data.get("node") or {}).get("fields")) or {}
        for field in conn.get("nodes", []):
            if field.get("__typename") in FIELD_TYPE_NAMES and field.get("id") and field.get("name"):
                fields[field["name"]] = field

        if not conn.get("pageInfo", {}).get("hasNextPage"):
            break
        after = conn["pageInfo"]["endCursor"]

    return fields


def fetch_source_items(token: Optional[str], project_id: str, source_field: str) -> list:
    query = """
    query($project_id: ID!, $field_name: String!, $after: String) {
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
                  id
                  number
                  title
                  url
                  repository {
                    nameWithOwner
                  }
                }
              }
              source: fieldValueByName(name: $field_name) {
                __typename
                ... on ProjectV2ItemFieldTextValue {
                  text
                }
                ... on ProjectV2ItemFieldNumberValue {
                  number
                }
                ... on ProjectV2ItemFieldDateValue {
                  date
                }
                ... on ProjectV2ItemFieldSingleSelectValue {
                  name
                }
                ... on ProjectV2ItemFieldIterationValue {
                  title
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
        data = graphql(token, query, {"project_id": project_id, "field_name": source_field, "after": after})
        conn = ((data.get("node") or {}).get("items")) or {}
        items.extend(conn.get("nodes", []))

        if not conn.get("pageInfo", {}).get("hasNextPage"):
            break
        after = conn["pageInfo"]["endCursor"]

    return items


def fetch_target_items(token: Optional[str], project_id: str, target_field: str) -> Dict[str, Any]:
    query = """
    query($project_id: ID!, $field_name: String!, $after: String) {
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
                  id
                  number
                  title
                  url
                  repository {
                    nameWithOwner
                  }
                }
              }
              target: fieldValueByName(name: $field_name) {
                __typename
                ... on ProjectV2ItemFieldTextValue {
                  text
                }
                ... on ProjectV2ItemFieldNumberValue {
                  number
                }
                ... on ProjectV2ItemFieldDateValue {
                  date
                }
                ... on ProjectV2ItemFieldSingleSelectValue {
                  name
                }
                ... on ProjectV2ItemFieldIterationValue {
                  title
                }
              }
            }
          }
        }
      }
    }
    """

    items = {}
    after = None
    while True:
        data = graphql(token, query, {"project_id": project_id, "field_name": target_field, "after": after})
        conn = ((data.get("node") or {}).get("items")) or {}
        for item in conn.get("nodes", []):
            content = item.get("content") or {}
            if content.get("__typename") == "Issue" and content.get("id"):
                items[content["id"]] = item

        if not conn.get("pageInfo", {}).get("hasNextPage"):
            break
        after = conn["pageInfo"]["endCursor"]

    return items


def normalized_value(field_value: Optional[Dict[str, Any]]) -> Any:
    if not field_value:
        return None

    typename = field_value.get("__typename")
    if typename == "ProjectV2ItemFieldTextValue":
        return field_value.get("text")
    if typename == "ProjectV2ItemFieldNumberValue":
        return field_value.get("number")
    if typename == "ProjectV2ItemFieldDateValue":
        return field_value.get("date")
    if typename == "ProjectV2ItemFieldSingleSelectValue":
        return field_value.get("name")
    if typename == "ProjectV2ItemFieldIterationValue":
        return field_value.get("title")
    return None


def field_type(field: Dict[str, Any]) -> Optional[str]:
    typename = field.get("__typename")
    if typename == "ProjectV2SingleSelectField":
        return "SINGLE_SELECT"
    if typename == "ProjectV2IterationField":
        return "ITERATION"
    return field.get("dataType")


def make_field_value(target_field: Dict[str, Any], source_value: Any) -> Optional[Dict[str, Any]]:
    if source_value is None:
        return None

    target_type = field_type(target_field)
    if target_type == "TEXT":
        return {"text": str(source_value)}
    if target_type == "NUMBER":
        return {"number": float(source_value)}
    if target_type == "DATE":
        return {"date": str(source_value)}
    if target_type == "SINGLE_SELECT":
        for option in target_field.get("options", []):
            if option.get("name") == str(source_value):
                return {"singleSelectOptionId": option["id"]}
        raise RuntimeError(
            "Target field %s has no single-select option named %s"
            % (target_field["name"], source_value)
        )
    if target_type == "ITERATION":
        iterations = ((target_field.get("configuration") or {}).get("iterations")) or []
        for iteration in iterations:
            if iteration.get("title") == str(source_value):
                return {"iterationId": iteration["id"]}
        raise RuntimeError(
            "Target field %s has no iteration named %s"
            % (target_field["name"], source_value)
        )

    raise RuntimeError(
        "Target field %s has unsupported data type %s"
        % (target_field["name"], target_type)
    )


def issue_label(item: Dict[str, Any]) -> str:
    content = item.get("content") or {}
    repo = ((content.get("repository") or {}).get("nameWithOwner")) or ""
    number = content.get("number")
    title = content.get("title") or ""
    if repo and number is not None:
        return "%s#%s %s" % (repo, number, title)
    if number is not None:
        return "#%s %s" % (number, title)
    return title or item.get("id") or "unknown item"


def update_field(token: Optional[str], project_id: str, item_id: str, field_id: str, value: Dict[str, Any]) -> None:
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
            "value": value,
        },
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Copy a GitHub Project V2 custom field to another project.")
    parser.add_argument("--org", required=True, help="GitHub organization login")
    parser.add_argument("--source-project-id", required=True, help="Source GitHub Project V2 node id or numeric project number")
    parser.add_argument("--target-project-id", required=True, help="Target GitHub Project V2 node id or numeric project number")
    parser.add_argument("--source-field", required=True, help="Source custom field name")
    parser.add_argument("--target-field", required=True, help="Target custom field name")
    parser.add_argument("--token", help="GitHub token. Defaults to GITHUB_TOKEN or GH_TOKEN.")
    parser.add_argument("--dry-run", action="store_true", help="Print updates without writing them")
    args = parser.parse_args()

    token = args.token or os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")

    source_project = fetch_project(token, args.org, args.source_project_id)
    target_project = fetch_project(token, args.org, args.target_project_id)

    source_fields = fetch_project_fields(token, source_project["id"])
    target_fields = fetch_project_fields(token, target_project["id"])

    if args.source_field not in source_fields:
        raise RuntimeError(
            "Source project %s is missing field %s"
            % (source_project["title"], args.source_field)
        )
    if args.target_field not in target_fields:
        raise RuntimeError(
            "Target project %s is missing field %s"
            % (target_project["title"], args.target_field)
        )

    target_field = target_fields[args.target_field]
    source_items = fetch_source_items(token, source_project["id"], args.source_field)
    target_items = fetch_target_items(token, target_project["id"], args.target_field)

    changed = 0
    skipped = 0
    missing = 0
    unsupported = 0

    print("Source project: %s (%s) in %s" % (source_project["title"], source_project["id"], args.org))
    print("Target project: %s (%s) in %s" % (target_project["title"], target_project["id"], args.org))
    print("Copying %s -> %s" % (args.source_field, args.target_field))

    for source_item in source_items:
        source_content = source_item.get("content") or {}
        if source_content.get("__typename") != "Issue":
            unsupported += 1
            print("skip non-issue: %s" % issue_label(source_item))
            continue

        source_value = normalized_value(source_item.get("source"))
        if source_value is None:
            skipped += 1
            print("skip empty: %s" % issue_label(source_item))
            continue

        target_item = target_items.get(source_content.get("id"))
        if not target_item:
            missing += 1
            print("missing in target: %s" % issue_label(source_item))
            continue

        current_value = normalized_value(target_item.get("target"))
        if current_value == source_value:
            skipped += 1
            print("unchanged: %s -> %s: %s" % (issue_label(source_item), args.target_field, source_value))
            continue

        update_value = make_field_value(target_field, source_value)
        changed += 1
        print(
            "update: %s -> %s: %s (was %s)"
            % (issue_label(source_item), args.target_field, source_value, current_value)
        )
        if not args.dry_run:
            update_field(token, target_project["id"], target_item["id"], target_field["id"], update_value)

    print(
        "Processed %d source items, %d updated, %d skipped, %d missing in target, %d unsupported"
        % (len(source_items), changed, skipped, missing, unsupported)
    )
    if args.dry_run:
        print("Dry run only; no changes were written.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
