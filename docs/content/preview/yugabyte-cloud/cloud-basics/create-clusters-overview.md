---
title: Plan your cluster
linkTitle: Plan your cluster
description: Plan a cluster in YugabyteDB Managed.
headcontent: Before deploying a production cluster, consider the following factors
image: /images/section_icons/deploy/enterprise.png
menu:
  preview_yugabyte-cloud:
    identifier: create-clusters-overview
    parent: cloud-basics
    weight: 10
type: docs
---

## Best practices

The following best practices are recommended for production clusters.

| Feature | Recommendation |
| :--- | :--- |
| [Provider and region](#provider-and-region) | Deploy your cluster in a virtual private cloud (VPC), with the same provider and in the same region as your application VPC. YugabyteDB Managed supports AWS, Azure, and GCP.<br>Multi-region clusters must be deployed in VPCs. You need to create the VPCs before you deploy the cluster. Refer to [VPC network](../cloud-vpcs/). |
| [Fault tolerance](#fault-tolerance) | Region or Availability zone (AZ) level - minimum of three nodes across multiple regions or AZs, with a replication factor of 3. |
| [Sizing](#sizing) | For most production applications, at least 3 nodes with 4 to 8 vCPUs per node.<br>Clusters support 10 simultaneous connections per vCPU. For example, a 3-node cluster with 4 vCPUs per node can support 10 x 3 x 4 = 120 connections.<br>When scaling your cluster, for best results increase node size up to 16 vCPUs before adding more nodes. For example, for a 3-node cluster with 4 vCPUs per node, scale up to 8 or 16 vCPUs before adding a fourth node. |
| [YugabyteDB version](#yugabytedb-version) | Use the **Stable** release track. |
| [Staging cluster](#staging-cluster) | Use a staging cluster to test application compatibility with database updates before upgrading your production cluster. |
| [Backups](#backups) | Use the default backup schedule (daily, with 8 day retention). |
| [Security and authorization](#security) | YugabyteDB Managed clusters are secure by default. After deploying, set up IP allow lists and add database users to allow clients, applications, and application VPCs to connect. Refer to [IP allow lists](../../cloud-secure-clusters/add-connections/). |

## In depth

### Topology

A YugabyteDB cluster typically consists of three or more nodes that communicate with each other and across which data is distributed. You can place the nodes in a single availability zone, across multiple zones in a single region, and across regions. With more advanced topologies, you can place multiple clusters across multiple regions. The [topology](../create-clusters-topology/) you choose depends on your requirements for latency, availability, and geo-distribution.

#### Single Region

Single-region clusters are available in the following topologies:

- **Single availability zone**. Resilient to node outages.
- **Multiple availability zones**. Resilient to node and availability zone outages.

Cloud providers design zones to minimize the risk of correlated failures caused by physical infrastructure outages like power, cooling, or networking. In other words, single failure events usually affect only a single zone. By deploying nodes across zones in a region, you get resilience to a zone failure as well as high availability.

Single-region clusters are not resilient to region-level outages.

#### Multiple Region

Multi-region clusters are resilient to region-level outages, and are available in the following topologies:

- **Replicate across regions**. Cluster nodes are deployed across 3 regions, with data replicated synchronously.
- **Partition by region**. Cluster nodes are deployed in separate regions. Data is pinned to specific geographic regions. Allows fine-grained control over pinning rows in a user table to specific geographic locations.
- **Read replica**. Replica clusters are deployed in separate regions. Data is written in the primary cluster, and copied to the read replicas, where it can be read. The primary cluster gets all write requests, while read requests can go either to the primary cluster or to the read replica clusters depending on which is closest.
<!-- - **Cross-cluster**. Two clusters are deployed in separate regions. Data is shared between the clusters, either in one direction, or asynchronously. -->

Multi-region clusters must be deployed in VPCs, with each region or read replica deployed in its own VPC. Refer to [VPC networking](../cloud-vpcs/).

For more details, refer to [Topologies](../create-clusters-topology/).

### Provider and region

#### Provider

YugabyteDB Managed supports AWS, Azure, and GCP. Your choice of provider will depend primarily on where your existing applications are hosted. YugabyteDB Managed pricing is the same for all.

| Feature | AWS | Azure | GCP |
| :--- | :--- | :--- | :--- |
| Sandbox | Yes | No | Yes |
| VPC | Yes | Yes (Required) | Yes |
| Peering | Yes | No | Yes |
| Private Service Endpoint | Yes | Yes | No |
| Topologies | Single region<br/>Replicate across regions<br/>Partition by region | Single region | Single region<br/>Replicate across regions<br/>Partition by region |
| Read replicas | Yes | No | Yes |
| Customer Managed Key | Yes | No | Yes |

#### Region

For best performance as well as lower data transfer costs, you want to minimize transfers between providers, and between provider regions. Do this by locating your cluster as close to your applications as possible:

- Use the same cloud provider as your application.
- Locate your cluster in the same region as your application.

For lowest possible network latency and data transfer costs, deploy your cluster in a VPC on the same cloud provider as your application VPC and connect it to the application VPC via peering (AWS or GCP) or using a private link (AWS or Azure). This configuration also provides the best security. To connect using a private link (AWS and Azure only), the link endpoints (your cluster and the application) must be in the same region.

For a list of supported regions, refer to [Cloud provider regions](#cloud-provider-regions).

#### Instance types

An instance in cloud computing is a server resource provided by third-party cloud services. An instance abstracts physical computing infrastructure using virtual machines. It's similar to having your own server machine in the cloud.

Cloud providers offer a variety of instance types across the regions where they have data centers. By default, where possible, YugabyteDB Managed uses the following instance type families for dedicated clusters:

- AWS - m6i
- Azure - Dasv5
- GCP - n2-standard

In cases where the default is unavailable in a region, YugabyteDB Managed will fall back to a suitable replacement instance type for all nodes in the cluster.

### Fault tolerance

The _fault tolerance_ determines how resilient the cluster is to node and cloud zone failures. YugabyteDB Managed provides the following options for providing replication and redundancy:

- **Region Level**. Includes 3 nodes spread across multiple regions with a [replication factor](../../../architecture/docdb-replication/replication/) (RF) of 3. YugabyteDB can continue to do reads and writes even in case of a cloud region failure. This configuration provides the maximum protection for a regional failure.

- **Availability Zone Level**. Includes a minimum of 3 nodes spread across multiple availability zones with a RF of 3. YugabyteDB can continue to do reads and writes even in case of a cloud availability zone failure. This configuration provides the maximum protection for a data center failure.

- **Node Level**. Includes a minimum of 3 nodes deployed in a single availability zone with a RF of 3. YugabyteDB can continue to do reads and writes even in case of a node failure, but this configuration is not resilient to cloud availability zone outages.

Although you can't change the cluster fault tolerance after the cluster is created, you can scale horizontally as follows:

- For Region Level, you can add or remove nodes in increments of 1 per region; all regions have the same number of nodes.
- For Availability Zone Level, you can add or remove nodes in increments of 3.
- For Node Level, you can add or remove nodes in increments of 1.

For production clusters, a minimum of Availability Zone Level is recommended. Whether you choose Region or Availability Zone Level depends on your application architecture, design, and latency requirements.

For application development and testing, you can set fault tolerance to **None** to create a single-node cluster. Single-node clusters can't be scaled.

### Sizing

The size of the cluster is based on the number of vCPUs. The default configuration for YugabyteDB Managed clusters includes 4 vCPUs per node. Each vCPU comes with 50GB of storage. A node has a minimum of 2 vCPUs with 4GB of memory per vCPU. For the cluster to be [fault tolerant](#fault-tolerance), you need a minimum of 3 nodes.

YugabyteDB Managed clusters support 10 simultaneous connections per vCPU. So a cluster with 3 nodes and 4 vCPUs per node can support 10 x 3 x 4 = 120 simultaneous connections.

| Cluster size (node x vCPU) | Maximum simultaneous connections |
| :--- | :--- |
| 3x2 | 60 |
| 3x4 | 120 |
| 3x8 | 240 |
| 6x2 | 120 |
| 6x4 | 240 |
| 6x8 | 480 |

During an update, one node is always offline. When sizing your cluster to your workload, ensure you have enough additional capacity to support rolling updates with minimal impact on application performance. You can also mitigate the effect of updates on performance by [scheduling them](../../cloud-clusters/cloud-maintenance/) during periods of lower traffic.

YugabyteDB Managed supports both vertical and horizontal scaling. Depending on your performance requirements, you can increase the number of vCPUs per node, as well as the total number of nodes. You can also increase the disk size per node. However, once increased, you can't lower the disk size per node.

If your configuration doesn't match your performance requirements, you can change these values after the cluster is created (increasing or decreasing vCPUs and increasing storage, and adding or removing nodes). Refer to [Scaling clusters](../../cloud-clusters/configure-clusters/).

For production clusters, a minimum of 3 nodes with 4 to 8 vCPUs per node is recommended.

### YugabyteDB version

By default, clusters are created using a stable release, taken from the [stable release series](../../../releases/versioning/#stable-releases) of YugabyteDB.

You can choose to deploy your cluster using a preview release for development and testing. YugabyteDB Managed database preview releases are typically taken from the [preview release series](../../../releases/versioning/#preview-releases) of YugabyteDB, though they can also include a recently released stable release.

If you need a feature from a preview release (that isn't yet available in a stable release) for a production deployment, contact {{% support-cloud %}} before you create your cluster.

Yugabyte manages upgrades for you. After you choose a track, database upgrades continue to take releases from the track you chose. For multi-node clusters, Yugabyte performs a rolling upgrade without any downtime. You can manage when Yugabyte performs maintenance and upgrades by configuring the [maintenance window](../../cloud-clusters/cloud-maintenance/) for your cluster.

{{< warning title="Important" >}}

There is currently no migration path from a preview release to a stable release.

{{< /warning >}}

### Staging cluster

Use a staging cluster for the following tasks:

- Verifying that your application is compatible with [database updates](#database-updates).
- Ensuring that your application correctly handles a rolling restart of the database without errors.
- Testing new features. Use your staging (also known as development, testing, pre-production, or canary) environment to try out new database features while your production systems are still running a previous version.
- Testing scaling operations and disaster recovery. Find out how your environment responds to a scaling operation, outages, or the loss of a node.

Create a staging cluster and configure your staging environment to connect to it. The staging cluster can be smaller than your production cluster, but you need to ensure that it has enough resources and capacity to handle a reasonable load. Sandbox clusters are too [resource-limited](../create-clusters/create-clusters-free/#limitations) for staging.

#### Database upgrades

Every YugabyteDB version in the stable track is tested for backwards compatibility. However, before upgrading your production cluster, it's good practice to first test your pre-production environment against database updates to ensure your applications are compatible. You want to make sure that an update doesn't have any performance impact on your application. For example, new functionality or a change in the optimizer could impact performance for a single query in a complex application.

When you are notified of an upcoming maintenance event, schedule the [maintenance windows](../../cloud-clusters/cloud-maintenance/) for the staging and production cluster so that you can validate updates against your applications in your pre-production environment _before_ upgrading your production cluster.

You can also set an [exclusion period](../../cloud-clusters/cloud-maintenance/#set-a-maintenance-exclusion-period) for your production cluster to postpone upgrades while you conduct testing.

If you identify a performance problem or regression with an update, set an exclusion period for your production cluster and contact {{% support-cloud %}}.

### Backups

YugabyteDB Managed provides a default recommended backup schedule (daily, with 8 day retention), and manages backups for you. You can [change the default schedule](../../cloud-clusters/backup-clusters/#schedule-backups), as well as perform [on-demand backups](../../cloud-clusters/backup-clusters/#on-demand-backups).

YugabyteDB Managed performs full cluster (all namespaces) level backups, and the backups are stored in the same region as your cluster. 100GB/month of basic backup storage is provided for every vCPU; more than that and overage charges apply. Refer to [Backup storage costs](../../cloud-admin/cloud-billing-costs/#backup-storage-costs).

### Security

Clusters are secure by default. You need to explicitly allow access to clusters by adding IP addresses of clients connecting to the cluster to the cluster IP allow list. Refer to [IP allow lists](../../cloud-secure-clusters/add-connections/).

If your applications are running in a VPC, deploy your cluster in a VPC to improve security and lower network latency. If you are using peering, you also need to add the CIDR ranges of peered application VPCs to your cluster IP allow list.

Multi-region clusters must be deployed in VPCs; in AWS, each region or read replica must be deployed in its own VPC.

You need to create VPCs before you deploy the cluster. YugabyteDB Managed supports AWS, Azure, and GCP for VPCs. Refer to [VPC network](../cloud-vpcs/).

#### Database user authorization

YugabyteDB uses role-based access control to manage database access. When you create a cluster, YugabyteDB Managed adds a default database admin user (the credentials for this user are configurable).

After the cluster is provisioned, create a new database and [add users](../../cloud-secure-clusters/add-users/). You can create users specific to each connecting application, and restrict their access accordingly.

{{< note title="Note" >}}
In YSQL, the database admin user is not a full superuser. For security reasons, you do not have access to the Yugabyte or Postgres superusers, nor can you create users with superuser privileges.
{{< /note >}}

For more information on users and roles in YugabyteDB Managed, refer to [Database authorization in YugabyteDB Managed clusters](../../cloud-secure-clusters/cloud-users/).

## Pricing

The biggest factor in the price of a cluster is the number vCPUs.

Cluster charges are based on the total number of vCPUs used and how long they have been running. Cluster per-hour charges include [free allowances](../../cloud-admin/cloud-billing-costs/) for disk storage, backup storage, and data transfer. If you use more than the free allowance, you incur overages on top of the base vCPU capacity cost.

Before creating a cluster, you need to create your billing profile and add a payment method. Refer to [Manage your billing profile and payment method](../../cloud-admin/cloud-billing-profile/).

## Cloud provider regions

{{< tabpane text=true >}}

  {{% tab header="AWS" lang="aws" %}}

The following **AWS regions** are available:

- Cape Town (af-south-1)
- Hong Kong (ap-east-1)
- Tokyo (ap-northeast-1)
- Seoul (ap-northeast-2)
- Osaka (ap-northeast-3)
- Mumbai (ap-south-1)
- Singapore (ap-southeast-1)
- Sydney (ap-southeast-2)
- Central (ca-central-1)
- Frankfurt (eu-central-1)
- Stockholm (eu-north-1)
- Milan (eu-south-1)
- Ireland (eu-west-1)
- London (eu-west-2)
- Paris (eu-west-3)
- Bahrain (me-south-1)
- Sao Paulo (sa-east-1)
- N. Virginia (us-east-1)
- Ohio (us-east-2)
- N. California (us-west-1)*
- Oregon (us-west-2)

\* Region has 2 availability zones only

  {{% /tab %}}

  {{% tab header="Azure" lang="azure" %}}

The following **Azure regions** are available:

- New South Wales (australiaeast)
- Sao Paulo State (brazilsouth)
- Toronto (canadacentral)
- Pune (centralindia)
- Iowa (centralus)
- Hong Kong (eastasia)
- Virginia (eastus)
- Virginia (eastus2)
- Paris (francecentral)
- Tokyo, Saitama (japaneast)
- Seoul (koreacentral)
- Ireland (northeurope)
- Norway (norwayeast)
- Johannesburg (southafricanorth)
- Texas (southcentralus)
- Zurich (switzerlandnorth)
- Dubai (uaenorth)
- London (uksouth)
- Washington (westus2)
- Phoenix (westus3)

  {{% /tab %}}

  {{% tab header="GCP" lang="gcp" %}}

The following **GCP regions** are available:

- Taiwan (asia-east1)
- Honk Kong (asia-east2)
- Tokyo (asia-northeast1)
- Osaka (asia-northeast2)
- Seoul (asia-northeast3)
- Mumbai (asia-south1)
- Delhi (asia-south2)
- Singapore (asia-southeast1)
- Jakarta (asia-southeast2)
- Sydney (australia-southeast1)
- Melbourne (australia-southeast2)
- Warsaw (europe-central2)
- Finland (europe-north1)
- Belgium (europe-west1)
- London (europe-west2)
- Frankfurt (europe-west3)
- Netherlands (europe-west4)
- Zurich (europe-west6)
- Montreal (northamerica-northeast1)
- Toronto (northamerica-northeast2)
- Sao Paulo (southamerica-east1)
- Iowa (us-central1)
- South Carolina (us-east1)
- N. Virginia (us-east4)
- Oregon (us-west1)
- Los Angeles (us-west2)
- Salt Lake City (us-west3)
- Las Vegas (us-west4)

  {{% /tab %}}

{{< /tabpane >}}

## Next steps

- [Create a cluster](../create-clusters/)
