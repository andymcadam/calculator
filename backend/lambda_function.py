import ast
import json
import logging
import os
import time
import uuid
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
from typing import Any, Dict, List, Optional
from urllib.parse import parse_qs

import boto3
from boto3.dynamodb.conditions import Attr, Key

logger = logging.getLogger()
logger.setLevel(logging.INFO)

TABLE_NAME = os.environ.get("TABLE_NAME", "calculator-history")
DEFAULT_LIMIT = int(os.environ.get("DEFAULT_HISTORY_LIMIT", "50"))
MAX_LIMIT = int(os.environ.get("MAX_HISTORY_LIMIT", "500"))

_table = None

_ALLOWED_BINARY_OPS = {
    ast.Add: lambda a, b: a + b,
    ast.Sub: lambda a, b: a - b,
    ast.Mult: lambda a, b: a * b,
    ast.Div: lambda a, b: a / b,
}

_ALLOWED_UNARY_OPS = {ast.UAdd: lambda a: a, ast.USub: lambda a: -a}


class CalculatorError(Exception):
    pass


def _safe_decimal(value: Any) -> Decimal:
    try:
        return Decimal(str(value))
    except (InvalidOperation, ValueError, TypeError) as exc:
        raise CalculatorError("Invalid numeric value") from exc


def _eval_ast(node: ast.AST) -> Decimal:
    if isinstance(node, ast.Expression):
        return _eval_ast(node.body)

    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return _safe_decimal(node.value)

    if isinstance(node, ast.UnaryOp) and type(node.op) in _ALLOWED_UNARY_OPS:
        operand = _eval_ast(node.operand)
        return _ALLOWED_UNARY_OPS[type(node.op)](operand)

    if isinstance(node, ast.BinOp) and type(node.op) in _ALLOWED_BINARY_OPS:
        left = _eval_ast(node.left)
        right = _eval_ast(node.right)
        if isinstance(node.op, ast.Div) and right == 0:
            raise CalculatorError("Division by zero")
        return _ALLOWED_BINARY_OPS[type(node.op)](left, right)

    raise CalculatorError("Unsupported expression")


def evaluate_expression(expression: str) -> Decimal:
    if not expression or len(expression) > 256:
        raise CalculatorError("Expression must be 1..256 chars")

    try:
        parsed = ast.parse(expression, mode="eval")
    except SyntaxError as exc:
        raise CalculatorError("Invalid expression syntax") from exc

    return _eval_ast(parsed)


def _json_response(status_code: int, body: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "*",
            "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
        },
        "body": json.dumps(body, default=str),
    }


def _request_id(event: Dict[str, Any], context: Any) -> str:
    return (
        event.get("headers", {}).get("x-correlation-id")
        or event.get("requestContext", {}).get("requestId")
        or getattr(context, "aws_request_id", "unknown")
    )


def _route(event: Dict[str, Any]) -> str:
    if "rawPath" in event:
        return event.get("rawPath", "")
    return event.get("path", "")


def _method(event: Dict[str, Any]) -> str:
    if "requestContext" in event and "http" in event["requestContext"]:
        return event["requestContext"]["http"].get("method", "")
    return event.get("httpMethod", "")


def _parse_body(event: Dict[str, Any]) -> Dict[str, Any]:
    body = event.get("body") or "{}"
    if event.get("isBase64Encoded"):
        import base64

        body = base64.b64decode(body).decode("utf-8")
    return json.loads(body)


def _parse_query(event: Dict[str, Any]) -> Dict[str, str]:
    if event.get("queryStringParameters"):
        return event["queryStringParameters"]

    raw = event.get("rawQueryString") or ""
    if not raw:
        return {}

    parsed = parse_qs(raw)
    return {k: v[0] for k, v in parsed.items() if v}


def _pk(user_id: str, session_id: str) -> str:
    return f"USER#{user_id}#SESSION#{session_id}"


def _sk(timestamp_ms: int, calculation_id: str) -> str:
    return f"TS#{timestamp_ms:013d}#ID#{calculation_id}"


def _decimal_to_value(value: Decimal) -> str:
    normalized = value.normalize() if value != 0 else Decimal("0")
    return format(normalized, "f")

def _get_table():
    global _table
    if _table is None:
        _dynamodb = boto3.resource("dynamodb")
        _table = _dynamodb.Table(TABLE_NAME)
    return _table


def _log(level: str, payload: Dict[str, Any]) -> None:
    record = json.dumps(payload, default=str)
    if level == "error":
        logger.error(record)
    else:
        logger.info(record)


def handle_calculate(event: Dict[str, Any], req_id: str) -> Dict[str, Any]:
    body = _parse_body(event)
    expression = str(body.get("expression", "")).strip()
    user_id = str(body.get("userId", "anonymous")).strip() or "anonymous"
    session_id = str(body.get("sessionId", "default")).strip() or "default"

    result_decimal = evaluate_expression(expression)
    result = _decimal_to_value(result_decimal)

    calculation_id = str(uuid.uuid4())
    now = datetime.now(timezone.utc)
    ts_ms = int(time.time() * 1000)

    item = {
        "pk": _pk(user_id, session_id),
        "sk": _sk(ts_ms, calculation_id),
        "calculationId": calculation_id,
        "timestamp": now.isoformat(),
        "timestampMs": ts_ms,
        "userId": user_id,
        "sessionId": session_id,
        "expression": expression,
        "result": result,
        "requestId": req_id,
    }
    _get_table().put_item(Item=item)

    _log(
        "info",
        {
            "event": "calculation_saved",
            "requestId": req_id,
            "calculationId": calculation_id,
            "userId": user_id,
            "sessionId": session_id,
            "expression": expression,
            "result": result,
        },
    )

    return _json_response(
        200,
        {
            "requestId": req_id,
            "calculationId": calculation_id,
            "timestamp": item["timestamp"],
            "expression": expression,
            "result": result,
        },
    )


def handle_history(event: Dict[str, Any], req_id: str) -> Dict[str, Any]:
    query = _parse_query(event)
    user_id = (query.get("userId") or "anonymous").strip()
    session_id = (query.get("sessionId") or "default").strip()

    limit = DEFAULT_LIMIT
    try:
        if "limit" in query:
            limit = max(1, min(int(query["limit"]), MAX_LIMIT))
    except ValueError:
        raise CalculatorError("Invalid limit")

    key_expr = Key("pk").eq(_pk(user_id, session_id))
    query_args: Dict[str, Any] = {
        "KeyConditionExpression": key_expr,
        "ScanIndexForward": False,
        "Limit": limit,
    }

    start = query.get("start")
    end = query.get("end")
    if start and end:
        query_args["FilterExpression"] = Attr("timestamp").between(start, end)

    response = _get_table().query(**query_args)

    items: List[Dict[str, Any]] = []
    for item in response.get("Items", []):
        items.append(
            {
                "calculationId": item.get("calculationId"),
                "timestamp": item.get("timestamp"),
                "userId": item.get("userId"),
                "sessionId": item.get("sessionId"),
                "expression": item.get("expression"),
                "result": item.get("result"),
                "requestId": item.get("requestId"),
            }
        )

    _log(
        "info",
        {
            "event": "history_read",
            "requestId": req_id,
            "count": len(items),
            "userId": user_id,
            "sessionId": session_id,
        },
    )

    return _json_response(200, {"requestId": req_id, "items": items})


def handler(event: Dict[str, Any], context: Any) -> Dict[str, Any]:
    req_id = _request_id(event, context)
    path = _route(event)
    method = _method(event).upper()

    _log(
        "info",
        {
            "event": "request_received",
            "requestId": req_id,
            "path": path,
            "method": method,
        },
    )

    if method == "OPTIONS":
        return _json_response(200, {"ok": True})

    try:
        if path.endswith("/health") and method == "GET":
            return _json_response(200, {"ok": True, "requestId": req_id})

        if path.endswith("/calculate") and method == "POST":
            return handle_calculate(event, req_id)

        if path.endswith("/history") and method == "GET":
            return handle_history(event, req_id)

        return _json_response(404, {"error": "Not found", "requestId": req_id})
    except CalculatorError as exc:
        _log(
            "error",
            {
                "event": "request_error",
                "requestId": req_id,
                "error": str(exc),
            },
        )
        return _json_response(400, {"error": str(exc), "requestId": req_id})
    except Exception as exc:  # nosec B110
        _log(
            "error",
            {
                "event": "request_error",
                "requestId": req_id,
                "error": "Internal server error",
                "details": str(exc),
            },
        )
        return _json_response(500, {"error": "Internal server error", "requestId": req_id})
