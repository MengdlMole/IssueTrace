---
问题编号: "{{issue.ticket}}"
问题状态: "{{issue.status}}"
服务: "{{issue.service}}"
版本: "{{issue.version}}"
处理人: "{{issue.assignee}}"
问题提出人: "{{issue.reporter}}"
提出时间: "{{issue.reported_at}}"
解决时间: "{{issue.resolved_at}}"
---

# {{issue.title}}

## 一、问题背景

{{issue.original_problem}}

## 二、处理过程

<!-- issuetrace:auto:timeline:start -->
{{timeline}}
<!-- issuetrace:auto:timeline:end -->

## 三、问题定位

<!-- 在此补充直接原因、根本原因和触发条件。 -->

## 四、解决方案

<!-- 在此补充临时处理、正式修复和变更内容。 -->

## 五、验证结果

<!-- 在此补充验证环境、方法、结果和遗留风险。 -->

## 六、问题结论

{{issue.conclusion}}

## 七、附件

<!-- issuetrace:auto:attachments:start -->
{{attachments}}
<!-- issuetrace:auto:attachments:end -->
