import sqlparse
import json
import os
from sqlparse.sql import IdentifierList, Identifier, Where, Function, Comparison, TokenList
from sqlparse.tokens import Keyword, DML

def extract_select_items(token):
    # Handles select list: can be IdentifierList or Identifier or Function
    select_items = []
    if isinstance(token, IdentifierList):
        for i in token.get_identifiers():
            select_items.extend(extract_select_items(i))
    elif isinstance(token, Identifier):
        # Check for function inside identifier (e.g., AVG(DISCOUNT) AS avg_discount)
        for t in token.tokens:
            if isinstance(t, Function):
                func_name = t.get_name()
                params = t.get_parameters()
                attribute = str(params[0]).strip() if params else None
                alias = token.get_alias()
                select_items.append({
                    "query_type": func_name,
                    "attribute": attribute,
                    "variable": alias
                })
                break
        else:
            # Not a function, just a column
            name = token.get_real_name()
            alias = token.get_alias()
            select_items.append({
                "query_type": None,
                "attribute": name,
                "variable": alias
            })
    elif isinstance(token, Function):
        func_name = token.get_name()
        params = token.get_parameters()
        attribute = str(params[0]).strip() if params else None
        select_items.append({
            "query_type": func_name,
            "attribute": attribute,
            "variable": None
        })
    return select_items

def extract_where_conditions(where_token):
    filters = []

    def recurse(tokens):
        i = 0
        while i < len(tokens):
            tok = tokens[i]

            # Case 1: Simple comparison like RETURNFLAG = 'A'
            if isinstance(tok, Comparison):
                parts = [t for t in tok.tokens if not t.is_whitespace]
                if len(parts) == 3 and parts[1].value == '=':
                    attr = parts[0].value.strip()
                    val = parts[2].value.strip().strip("'").strip('"')
                    filters.append({"attribute": attr, "condition": val})

            # Case 2: BETWEEN clause: attr BETWEEN val1 AND val2
            elif isinstance(tok, TokenList):
                sub_tokens = [t for t in tok.tokens if not t.is_whitespace]
                for j in range(len(sub_tokens) - 4):
                    if (
                        sub_tokens[j+1].ttype == Keyword and sub_tokens[j+1].value.upper() == 'BETWEEN'
                        and sub_tokens[j+3].ttype == Keyword and sub_tokens[j+3].value.upper() == 'AND'
                    ):
                        attr = str(sub_tokens[j]).strip()
                        val1 = str(sub_tokens[j+2]).strip()
                        val2 = str(sub_tokens[j+4]).strip()
                        filters.append({
                            "attribute": attr,
                            "condition": f"{val1} AND {val2}"
                        })
                recurse(tok.tokens)  # Keep recursing

            i += 1

    if isinstance(where_token, TokenList):
        recurse(where_token.tokens)

    return filters


def parse_sql(sql):
    parsed = sqlparse.parse(sql)
    if not parsed:
        return {"select": [], "filters": []}
    stmt = parsed[0]
    select_items = []
    filters = []
    from_seen = False
    for token in stmt.tokens:
        if token.is_whitespace:
            continue
        if token.ttype is DML and token.value.upper() == 'SELECT':
            continue
        if token.ttype is Keyword and token.value.upper() == 'FROM':
            from_seen = True
            continue
        if not from_seen:
            # This is the select list
            if isinstance(token, (IdentifierList, Identifier, Function)):
                select_items.extend(extract_select_items(token))
        if isinstance(token, Where):
            filters = extract_where_conditions(token)
    return {
        "select": select_items,
        "filters": filters
    }

def find_sql_files(root_dir):
    sql_files = []
    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith('.txt'):
                sql_files.append(os.path.join(dirpath, filename))
    return sql_files

if __name__ == "__main__":
    sql_dir = os.path.join(os.path.dirname(__file__), '../SQL_Queries')
    sql_files = find_sql_files(sql_dir)
    for sql_file in sql_files:
        with open(sql_file) as f:
            sql = f.read()
        parsed_output = parse_sql(sql)
        json_file = sql_file.rsplit('.', 1)[0] + '.json'
        with open(json_file, "w") as out:
            json.dump(parsed_output, out, indent=4)
    print(f"Parsed {len(sql_files)} SQL files and generated corresponding JSON files.")
