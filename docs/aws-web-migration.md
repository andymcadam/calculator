# AWS-Native Web Calculator Architecture

This repository now includes an AWS-native web implementation of the calculator with multi-region failover, persistent calculation history, CloudWatch observability, and Terraform/GitHub Actions deployment.

## What was added

- Browser frontend: `web/`
- Lambda backend API: `backend/lambda_function.py`
- Terraform IaC: `terraform/`
- GitHub Actions workflows: `.github/workflows/`

## Architecture

- **Frontend**: static assets in S3, distributed by CloudFront in each region.
- **Backend**: API Gateway HTTP API + Lambda in primary and secondary regions.
- **Data durability**: DynamoDB Global Table replicating calculator history across regions.
- **Failover**: Route53 health-check-based DNS failover from primary to secondary CloudFront endpoint, driven by backend health checks.
- **Logging/metrics**: structured JSON logs from Lambda + API access logs in CloudWatch + CloudWatch alarms for Lambda/API/DynamoDB replication latency.

## API contract

### `POST /calculate`
Calculates an expression and persists the event.

Request body:
```json
{
  "expression": "(2+3)*4",
  "userId": "demo-user",
  "sessionId": "demo-session"
}
```

Response:
```json
{
  "requestId": "...",
  "calculationId": "...",
  "timestamp": "...",
  "expression": "(2+3)*4",
  "result": "20"
}
```

### `GET /history?userId=...&sessionId=...&limit=20`
Returns recent persisted calculations for that user/session.

### `GET /health`
Health endpoint used by Route53 checks.

## Terraform deployment notes

### Required input variables

- `route53_zone_id`
- `route53_record_name`

### Example usage

```bash
cd terraform
terraform init
terraform plan \
  -var="route53_zone_id=Z123456789" \
  -var="route53_record_name=calc.example.com"
terraform apply \
  -var="route53_zone_id=Z123456789" \
  -var="route53_record_name=calc.example.com"
```

## GitHub Actions

- `CI`: backend unit tests.
- `Terraform`: fmt/check/validate for IaC changes.
- `Deploy`: manual deploy via OIDC role assumption (`AWS_DEPLOY_ROLE_ARN` secret required).

## Failover behavior

If primary region health checks fail, Route53 directs users to the secondary region frontend. The secondary backend continues writing/reading from the local DynamoDB replica, and replication keeps saved data durable across regions.
