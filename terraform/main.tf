locals {
  name_prefix = "${var.project_name}-${var.environment}"
}

module "global_data" {
  source = "./modules/global_data"

  providers = {
    aws = aws.primary
  }

  table_name       = "${local.name_prefix}-history"
  secondary_region = var.secondary_region
}

module "regional_primary" {
  source = "./modules/regional_stack"

  providers = {
    aws = aws.primary
  }

  name_prefix                  = "${local.name_prefix}-primary"
  table_name                   = module.global_data.table_name
  region                       = var.primary_region
  cloudwatch_log_retention_days = var.cloudwatch_log_retention_days
  lambda_source_file           = "${path.root}/../backend/lambda_function.py"
  frontend_dir                 = "${path.root}/../web"
  history_default_limit        = var.history_default_limit
  history_max_limit            = var.history_max_limit
}

module "regional_secondary" {
  source = "./modules/regional_stack"

  providers = {
    aws = aws.secondary
  }

  name_prefix                  = "${local.name_prefix}-secondary"
  table_name                   = module.global_data.table_name
  region                       = var.secondary_region
  cloudwatch_log_retention_days = var.cloudwatch_log_retention_days
  lambda_source_file           = "${path.root}/../backend/lambda_function.py"
  frontend_dir                 = "${path.root}/../web"
  history_default_limit        = var.history_default_limit
  history_max_limit            = var.history_max_limit
}

resource "aws_route53_health_check" "primary_api" {
  fqdn              = trimprefix(module.regional_primary.api_invoke_url, "https://")
  port              = 443
  type              = "HTTPS"
  resource_path     = "/health"
  request_interval  = 30
  failure_threshold = 3
}

resource "aws_route53_health_check" "secondary_api" {
  fqdn              = trimprefix(module.regional_secondary.api_invoke_url, "https://")
  port              = 443
  type              = "HTTPS"
  resource_path     = "/health"
  request_interval  = 30
  failure_threshold = 3
}

resource "aws_route53_record" "frontend_primary" {
  zone_id = var.route53_zone_id
  name    = var.route53_record_name
  type    = "CNAME"
  ttl     = 60

  set_identifier  = "primary-cloudfront"
  health_check_id = aws_route53_health_check.primary_api.id

  failover_routing_policy {
    type = "PRIMARY"
  }

  records = [module.regional_primary.cloudfront_domain_name]
}

resource "aws_route53_record" "frontend_secondary" {
  zone_id = var.route53_zone_id
  name    = var.route53_record_name
  type    = "CNAME"
  ttl     = 60

  set_identifier  = "secondary-cloudfront"
  health_check_id = aws_route53_health_check.secondary_api.id

  failover_routing_policy {
    type = "SECONDARY"
  }

  records = [module.regional_secondary.cloudfront_domain_name]
}

resource "aws_cloudwatch_metric_alarm" "replication_latency" {
  alarm_name          = "${local.name_prefix}-dynamodb-replication-latency"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 3
  metric_name         = "ReplicationLatency"
  namespace           = "AWS/DynamoDB"
  period              = 60
  statistic           = "Maximum"
  threshold           = 30000
  treat_missing_data  = "notBreaching"

  dimensions = {
    TableName       = module.global_data.table_name
    ReceivingRegion = var.secondary_region
  }

  alarm_description = "Alerts when DynamoDB global table replication latency increases"
}
