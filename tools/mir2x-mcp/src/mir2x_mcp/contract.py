"""Strict validation shared by the IPC client and the MCP boundary."""

import copy
import json
import math
from functools import cache
from importlib.resources import files

from jsonschema import Draft202012Validator, ValidationError, validators


class ContractError(ValueError):
    """Never include rejected values: they may contain credentials or game input."""


def _utf8(validator, maximum, value, schema):
    if isinstance(value, str):
        try:
            valid = len(value.encode("utf-8")) <= maximum
        except UnicodeEncodeError:
            valid = False
        if not valid:
            yield ValidationError("UTF-8 length")


def _decimal(validator, maximum, value, schema):
    if isinstance(value, str) and (not value.isascii() or not value.isdecimal()):
        yield ValidationError("decimal string")
    elif isinstance(value, str) and (len(value) > 20 or int(value) > maximum):
        yield ValidationError("decimal maximum")


StrictValidator = validators.extend(
    Draft202012Validator,
    {"x-maxUtf8Bytes": _utf8, "x-decimalMaximum": _decimal},
    type_checker=Draft202012Validator.TYPE_CHECKER.redefine(
        "integer", lambda checker, value: type(value) is int
    ),
)
CONTRACT = json.loads(files("mir2x_mcp").joinpath("contract.schema.json").read_text("utf-8"))
METHODS = CONTRACT["x-methods"]


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ContractError("duplicate_key")
        result[key] = value
    return result


def _bad_constant(value):
    raise ContractError("non_finite")


def _depth(value, depth=0):
    if depth > 16:
        raise ContractError("json_depth")
    if isinstance(value, dict):
        for key, item in value.items():
            key.encode("utf-8")
            _depth(item, depth + 1)
    elif isinstance(value, list):
        for item in value:
            _depth(item, depth + 1)
    elif isinstance(value, str):
        value.encode("utf-8")
    elif isinstance(value, float) and not math.isfinite(value):
        raise ContractError("non_finite")


def loads(data: bytes):
    try:
        value = json.loads(
            data.decode("utf-8"), object_pairs_hook=_pairs, parse_constant=_bad_constant
        )
        _depth(value)
        return value
    except (ValueError, UnicodeError, RecursionError):
        raise ContractError("invalid_json") from None


def dumps(value) -> bytes:
    try:
        _depth(value)
        return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode(
            "utf-8"
        )
    except (ValueError, UnicodeError, RecursionError):
        raise ContractError("invalid_json") from None


def standalone(schema: dict) -> dict:
    """Include only reachable definitions, keeping tools/list reasonably small."""
    result = copy.deepcopy(schema)
    definitions = {}

    def walk(node):
        if isinstance(node, dict):
            if "$ref" in node:
                name = node["$ref"].removeprefix("#/$defs/")
                if name not in definitions:
                    definitions[name] = copy.deepcopy(CONTRACT["$defs"][name])
                    walk(definitions[name])
            for child in node.values():
                walk(child)
        elif isinstance(node, list):
            for child in node:
                walk(child)

    walk(result)
    if definitions:
        result["$defs"] = definitions
    return result


@cache
def definition_validator(name: str):
    return StrictValidator(standalone(CONTRACT["$defs"][name]))


def validate(value, name: str):
    if next(definition_validator(name).iter_errors(value), None) is not None:
        raise ContractError("schema_mismatch")
    return value


@cache
def input_schema(method: str) -> dict:
    definition = CONTRACT["$defs"][method + "Request"]
    common = {
        key: value
        for key, value in definition["properties"].items()
        if key not in {"v", "id", "method", "params"}
    }
    params = definition["properties"]["params"]
    branches = params.get("oneOf", [params])
    combined = [
        {
            "type": "object",
            "properties": branch["properties"] | common,
            "required": branch["required"] + list(common),
            "additionalProperties": False,
        }
        for branch in branches
    ]
    if len(combined) == 1:
        return standalone(combined[0])
    # MCP requires an object root even for the mutually exclusive cast targets.
    return standalone({"type": "object", "oneOf": combined})


@cache
def argument_validator(method: str):
    return StrictValidator(input_schema(method))


def validate_arguments(method: str, arguments: dict):
    if method not in METHODS or next(argument_validator(method).iter_errors(arguments), None):
        raise ContractError("invalid_argument")


@cache
def output_schema(method: str) -> dict:
    return standalone(CONTRACT["$defs"][method + "Response"]["oneOf"][0])
