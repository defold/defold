#!/usr/bin/env python3
"""
Export GitHub Projects (v2) items + custom fields to a CSV.

Example:
  python export_github_project_to_csv.py --token ghtoken --project 12 --out project.csv
  
Notes:
- This exports ALL items (pagination supported).
- Custom fields become CSV columns automatically.
"""


import argparse
import csv
import os
import sys

sys.path.append(os.path.join(os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')), "build_tools"))
import github


def graphql(token, query):
    response = github.query(query, token)
    if 'errors' in response:
        print(response)
        for error in response['errors']:
            print(error['message'])
        sys.exit(1)
    return response["data"]


def get_project_id(token, project_number):
    query = r"""
    {
        organization(login: "defold") {
            projectV2(number:%s) {
                id
                title
                number
            }
        }
    }
    """
    data = graphql(token, query % project_number)

    proj = data.get("organization", {}).get("projectV2")

    if not proj or not proj.get("id"):
        raise RuntimeError(f"Could not find ProjectV2 number {project_number}")

    print(f"Found project: {proj.get('title')}")
    return proj["id"]


def get_project_fields(token, project_id):
    """
    Returns mapping: field_id -> field_name
    """
    query = """
    query {
      node(id:"%s") {
        ... on ProjectV2 {
          fields(first:100, after:%s) {
            pageInfo { hasNextPage endCursor }
            nodes {
              __typename
              ... on ProjectV2FieldCommon { id name }
            }
          }
        }
      }
    }
    """
    field_map = {}
    after = None
    while True:
        data = graphql(token, query % (project_id, ("\"" + after + "\"") if after else "null"))
        fields = data["node"]["fields"]
        for n in fields["nodes"]:
            # Some nodes might not include name/id if not common, but most do.
            fid = n.get("id")
            fname = n.get("name")
            if fid and fname:
                field_map[fid] = fname
        if not fields["pageInfo"]["hasNextPage"]:
            break
        after = fields["pageInfo"]["endCursor"]
    return field_map


def normalize_field_value(field_value):
    """
    Converts ProjectV2ItemFieldValue union into a CSV-friendly primitive.
    """
    t = field_value.get("__typename")
    if t == "ProjectV2ItemFieldTextValue":
        return field_value.get("text")
    if t == "ProjectV2ItemFieldNumberValue":
        return field_value.get("number")
    if t == "ProjectV2ItemFieldDateValue":
        return field_value.get("date")
    if t == "ProjectV2ItemFieldSingleSelectValue":
        opt = field_value.get("name")
        return opt
    if t == "ProjectV2ItemFieldIterationValue":
        it = field_value.get("title")
        return it
    # Fallback: try common keys
    for k in ("text", "name", "number", "date", "title"):
        if k in field_value:
            return field_value.get(k)
    return None


def fetch_project_items(token, project_id, field_map):
    """
    Fetch all items and return (rows, custom_field_names).
    Each row includes base columns + one key per custom field name.
    """
    query = """
    query {
      node(id:"%s") {
        ... on ProjectV2 {
          items(first:100, after:%s) {
            pageInfo { hasNextPage endCursor }
            nodes {
              id
              createdAt
              updatedAt

              fieldValues(first:100) {
                nodes {
                  __typename
                  ... on ProjectV2ItemFieldTextValue { text field { ... on ProjectV2FieldCommon { id name } } }
                  ... on ProjectV2ItemFieldNumberValue { number field { ... on ProjectV2FieldCommon { id name } } }
                  ... on ProjectV2ItemFieldDateValue { date field { ... on ProjectV2FieldCommon { id name } } }
                  ... on ProjectV2ItemFieldSingleSelectValue { name field { ... on ProjectV2FieldCommon { id name } } }
                  ... on ProjectV2ItemFieldIterationValue { title field { ... on ProjectV2FieldCommon { id name } } }
                }
              }

              content {
                __typename
                ... on Issue {
                  id
                  number
                  title
                  url
                  state
                  createdAt
                  updatedAt
                  closedAt
                  repository { nameWithOwner }
                  author { login }
                  assignees(first:50) { nodes { login } }
                  labels(first:50) { nodes { name } }
                  milestone { title dueOn }
                }
                ... on PullRequest {
                  id
                  number
                  title
                  url
                  state
                  createdAt
                  updatedAt
                  closedAt
                  mergedAt
                  repository { nameWithOwner }
                  author { login }
                  assignees(first:50) { nodes { login } }
                  labels(first:50) { nodes { name } }
                  milestone { title dueOn }
                }
                ... on DraftIssue {
                  id
                  title
                  body
                  createdAt
                  updatedAt
                  assignees(first:50) { nodes { login } }
                }
              }
            }
          }
        }
      }
    }
    """

    base_cols = [
        "project_item_id",
        "item_type",
        "title",
        "url",
        "state",
        "repo",
        "number",
        "author",
        "assignees",
        "labels",
        "milestone",
        "milestone_due",
        "created_at",
        "updated_at",
        "closed_at",
        "merged_at",
    ]

    all_rows = []
    custom_names = set(field_map.values())

    after = None
    while True:
        data = graphql(token, query % ( project_id, ("\"" + after + "\"") if after else "null"))
        items = data["node"]["items"]

        for item in items["nodes"]:
            row = {c: "" for c in base_cols}
            # initialize custom fields
            for cname in custom_names:
                row[cname] = ""

            row["project_item_id"] = item.get("id", "")
            row["created_at"] = item.get("createdAt", "")
            row["updated_at"] = item.get("updatedAt", "")

            content = item.get("content")
            if content:
                ctype = content.get("__typename", "")
                row["item_type"] = ctype

                if ctype == "Issue":
                    row["title"] = content.get("title", "")
                    row["url"] = content.get("url", "")
                    row["state"] = content.get("state", "")
                    row["repo"] = (content.get("repository") or {}).get("nameWithOwner", "")
                    row["number"] = content.get("number", "")
                    row["author"] = (content.get("author") or {}).get("login", "")
                    row["assignees"] = ",".join([n["login"] for n in (content.get("assignees") or {}).get("nodes", []) if n.get("login")])
                    row["labels"] = ",".join([n["name"] for n in (content.get("labels") or {}).get("nodes", []) if n.get("name")])
                    ms = content.get("milestone")
                    if ms:
                        row["milestone"] = ms.get("title", "")
                        row["milestone_due"] = ms.get("dueOn", "") or ""
                    row["closed_at"] = content.get("closedAt", "") or ""

                elif ctype == "PullRequest":
                    row["title"] = content.get("title", "")
                    row["url"] = content.get("url", "")
                    row["state"] = content.get("state", "")
                    row["repo"] = (content.get("repository") or {}).get("nameWithOwner", "")
                    row["number"] = content.get("number", "")
                    row["author"] = (content.get("author") or {}).get("login", "")
                    row["assignees"] = ",".join([n["login"] for n in (content.get("assignees") or {}).get("nodes", []) if n.get("login")])
                    row["labels"] = ",".join([n["name"] for n in (content.get("labels") or {}).get("nodes", []) if n.get("name")])
                    ms = content.get("milestone")
                    if ms:
                        row["milestone"] = ms.get("title", "")
                        row["milestone_due"] = ms.get("dueOn", "") or ""
                    row["closed_at"] = content.get("closedAt", "") or ""
                    row["merged_at"] = content.get("mergedAt", "") or ""

                elif ctype == "DraftIssue":
                    row["title"] = content.get("title", "")
                    # Draft issues don't have a canonical URL
                    row["url"] = ""
                    row["state"] = "DRAFT"
                    row["repo"] = ""
                    row["number"] = ""
                    row["author"] = ""
                    row["assignees"] = ",".join([n["login"] for n in (content.get("assignees") or {}).get("nodes", []) if n.get("login")])
                    row["labels"] = ""
                    row["milestone"] = ""
                    row["milestone_due"] = ""
                    row["closed_at"] = ""
                    row["merged_at"] = ""

            # Custom fields: map by field name
            fv_nodes = (item.get("fieldValues") or {}).get("nodes", [])
            for fv in fv_nodes:
                field = fv.get("field") or {}
                fname = field.get("name")
                if not fname:
                    # sometimes only id is available, fall back to field_map
                    fid = field.get("id")
                    fname = field_map.get(fid) if fid else None
                if fname:
                    row[fname] = normalize_field_value(fv) or ""

            all_rows.append(row)

        if not items["pageInfo"]["hasNextPage"]:
            break
        after = items["pageInfo"]["endCursor"]

    # final column order
    custom_field_cols = sorted(custom_names)
    columns = base_cols + custom_field_cols
    return all_rows, columns


def write_csv(path, rows, columns):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=columns, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            # ensure all values are CSV-safe strings
            out = {}
            for k in columns:
                v = r.get(k, "")
                if v is None:
                    v = ""
                out[k] = v
            w.writerow(out)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--token", required=True, help="GitHub token")
    p.add_argument("--project", type=int, required=True, help="Project number (shown in the URL)")
    p.add_argument("--out", required=True, help="Output CSV filename")
    args = p.parse_args()

    project_id = get_project_id(args.token, args.project)
    field_map = get_project_fields(args.token, project_id)
    rows, columns = fetch_project_items(args.token, project_id, field_map)

    write_csv(args.out, rows, columns)
    print("Wrote %d items to %s" % (len(rows), args.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
