variable "project_name" {
  description = "Project name used for resource naming"
  type        = string
  default     = "calculator"
}

variable "environment" {
  description = "Deployment environment"
  type        = string
  default     = "prod"
}

variable "primary_region" {
  description = "Primary AWS region"
  type        = string
  default     = "us-east-1"
}

variable "secondary_region" {
  description = "Secondary AWS region for failover"
  type        = string
  default     = "us-west-2"
}

variable "route53_zone_id" {
  description = "Hosted zone ID for failover DNS records"
  type        = string
}

variable "route53_record_name" {
  description = "DNS record name (typically a subdomain) for CloudFront failover"
  type        = string
}

variable "cloudwatch_log_retention_days" {
  description = "CloudWatch log retention in days"
  type        = number
  default     = 30
}

variable "history_default_limit" {
  description = "Default API history page size"
  type        = number
  default     = 50
}

variable "history_max_limit" {
  description = "Maximum API history page size"
  type        = number
  default     = 500
}
