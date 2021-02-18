# This dict describes the fields in each message type which adhere to the built-in
# aggregation scheme. The format is
#
# "message_type" : {raw_field_num:aggregated_field_num}
#
# It is expected that the type of the aggregated field num contains at least weight, max,
# min, and sum.
PRIMARY_KEY =  99999999 #These should probably be an enum. SMAGENT-1976
OR =           99999998
LIMITED = 99999997
UNIQUE = 99999996

field_extension = {
    "app_metric_bucket" : {1: {PRIMARY_KEY},
                           2:3},
    "counter_percentile" : {1: {PRIMARY_KEY},
                            2:3},
    "counter_time" : {1:4,
                      2:5,
                      3:6},
    "counter_time_bidirectional" : {1:5,
                                    2:6,
                                    3:7,
                                    4:8},
    "counter_bytes" : {1:5,
                       2:6,
                       3:7,
                       4:8},
    "counter_time_bytes" : {1:13,
                            2:14,
                            3:15,
                            4:16,
                            5:17,
                            6:18,
                            7:19,
                            8:20,
                            9:21,
                            10:22,
                            11:23,
                            12:24},
    "counter_syscall_errors" : {1:3,
                                4:7,
                                5:8,
                                6:9},
    "resource_categories" : {1:12,
                             6:7,
                             2:8,
                             3:9,
                             4:10,
                             5:11,
                             13:17,
                             14:18,
                             15:19,
                             16:20,
                             21:22,
                             24:124,
                             25:125,
                             26:126,
                             27:127,
                             28:128,
                             29:129,
                             30:130,
                             31:131,
                             32:132,
                             33:133,
                             34:134,
                             35:135,
                             36:136,
                             37:137,
                             38:138,
                             39:139,
                             40:140,
                             41:141,
                             42:142,
                             43:143,
                             44:144,
                             45:145,
                             46:146,
                             47:147},
    "connection_categories" : {4:5},
    "counter_proto_entry" : {1:7,
                             2:8,
                             3:9,
                             4:10,
                             5:11,
                             6:12},
    "status_code_details" : {2:3},
    "url_details" : {1: {PRIMARY_KEY}},
    "status_code_details" : {1: {PRIMARY_KEY},
                             2:3},
    "http_info" : {1: {LIMITED},
                   2: {LIMITED},
                   3: {LIMITED},
                   4: {LIMITED}},
    "sql_entry_details" : {1: {PRIMARY_KEY}},
    "sql_query_type_details" : {1: {PRIMARY_KEY}},
    "sql_info" : {1: {LIMITED},
                  2: {LIMITED},
                  3: {LIMITED},
                  4: {LIMITED},
                  5: {LIMITED},
                  6: {LIMITED}},
    "mongodb_collection_details" : {1: {PRIMARY_KEY}},
    "mongodb_op_type_details" : {1: {PRIMARY_KEY}},
    "mongodb_info" : {1: {LIMITED},
                      2: {LIMITED},
                      3: {LIMITED},
                      4: {LIMITED}},
    "statsd_tag" : {1: {PRIMARY_KEY},
                    2: {PRIMARY_KEY}},
    "jmx_attribute" : {1: {PRIMARY_KEY},
                       2:6}, # note there are two aggregations, but only one is used
    "jmx_bean" : {1: {PRIMARY_KEY}},
    "statsd_metric" : {1: {PRIMARY_KEY},
                       2: {PRIMARY_KEY},
                       4:104,
                       5:105,
                       6:106,
                       7:107,
                       8:108,
                       9:109,
                       10:110,
                       11:111},
    "statsd_info" : {1: {LIMITED}},
    "app_tag" : {1: {PRIMARY_KEY},
                 2: {PRIMARY_KEY}}, # SMAGENT-2624
    "app_metric" : {1: {PRIMARY_KEY},
                    3:103,
                    4: {PRIMARY_KEY}},
    "app_check" : {1: {PRIMARY_KEY}},
    "app_info" : {2: {LIMITED}},
    "prom_metric" : {1: {PRIMARY_KEY},
                     3:103,
                     4: {PRIMARY_KEY, UNIQUE}},
    "prometheus_info" : {2: {LIMITED, UNIQUE}},
    "network_by_port" : {1: {PRIMARY_KEY}},
    "host" : {2:21,
              3:15,
              4:17,
              11:16,
              7:18,
              13:20,
              24: {LIMITED},
              36:136,
              32:132,
              33:133,
              34:134,
              35:135,
              25:125,
              37:137,
              38:138,
              41:141,
              42:142,
              43:143,
              44:144},
    "process" : {7:15,
                 12:17,
                 13: OR,
                 21:22,
                 23:123,
                 24: {LIMITED},
                 25: {LIMITED}},
    "program" : {6: {PRIMARY_KEY}},
    "id_map" : {1: {PRIMARY_KEY}},
    "environment" : {1: {PRIMARY_KEY}},
    "ipv4tuple" : {1: {PRIMARY_KEY},
                   2: {PRIMARY_KEY},
                   3: {PRIMARY_KEY},
                   4: {PRIMARY_KEY},
                   5: {PRIMARY_KEY}},
    "ipv4_connection" : {1: {PRIMARY_KEY},
                         2: {PRIMARY_KEY},
                         5: {PRIMARY_KEY}},
    "ipv4_incomplete_connection" : {1: {PRIMARY_KEY},
                                    2: {PRIMARY_KEY}},
    "ipv4_network_interface" : {2: {PRIMARY_KEY},
                                3: {PRIMARY_KEY},
                                4: {PRIMARY_KEY}},
    "mounted_fs" : {2: {PRIMARY_KEY},
                    4:7,
                    5:8,
                    6:9,
                    10:110,
                    11:111},
    "file_stat" : {1: {PRIMARY_KEY},
                   2:9,
                   3:6,
                   4:7,
                   5:8,
                   10:12,
                   11:13},
    "container_port_mapping" : {1: {PRIMARY_KEY},
                                2: {PRIMARY_KEY},
                                3: {PRIMARY_KEY},
                                4: {PRIMARY_KEY}},
    "container_label" : {1: {PRIMARY_KEY},
                         2: {PRIMARY_KEY}},
    "container" : {1: {PRIMARY_KEY},
                   10:15,
                   12:17,
                   21: {LIMITED},
                   22: {LIMITED},
                   31: {LIMITED},
                   32: {LIMITED}},
    "k8s_pair" : {1: {PRIMARY_KEY}},
    "k8s_common" : {2: {PRIMARY_KEY}},
    "k8s_namespace" : {1: {PRIMARY_KEY},
                       2:102,
                       3:103,
                       4:104,
                       5:105,
                       6:106,
                       7:107,
                       9:109,
                       10:110,
                       11:111,
                       12:112},
    "k8s_node" : {1: {PRIMARY_KEY},
                  3:103,
                  4:104,
                  5:105,
                  6:106,
                  7:107,
                  8:108,
                  9:109,
                  10:110,
                  11:111,
                  12:112,
                  13:113,
                  14:114},
    "k8s_pod" : {1: {PRIMARY_KEY},
                 6:106,
                 7:107,
                 9:108, # the protobuf has an oopsie
                 10:109,
                 11:110,
                 12:111,
                 13:112,
                 14:113},
    "k8s_replication_controller" : {1: {PRIMARY_KEY},
                                    2:102,
                                    3:103},
    "k8s_service" : {1: {PRIMARY_KEY}},
    "k8s_replica_set" : {1: {PRIMARY_KEY},
                         2:102,
                         3:103,
                         4:104,
                         5:105},
    "k8s_deployment" : {1: {PRIMARY_KEY},
                        2:102,
                        3:103,
                        4:104,
                        5:105,
                        6:106,
                        7:107},
    "k8s_daemonset" : {1: {PRIMARY_KEY},
                       2:102,
                       3:103,
                       4:104,
                       5:105},
    "k8s_job" : {1: {PRIMARY_KEY},
                 2:102,
                 3:103,
                 4:104,
                 5:105,
                 6:106},
    "k8s_statefulset" : {1: {PRIMARY_KEY},
                         2:102,
                         3:103,
                         4:104,
                         5:105,
                         6:106},
    "k8s_resourcequota" : {1: {PRIMARY_KEY},
                           2:31,
                           3:32,
                           4:33,
                           5:34,
                           6:35,
                           7:36,
                           8:37,
                           9:38,
                           10:39,
                           11:40,
                           12:41,
                           13:42,
                           101:131, #don't ask...
                           102:132,
                           201:231,
                           202:232,
                           203:233,
                           204:234,
                           205:235,
                           206:236,
                           207:237,
                           208:238,
                           209:239,
                           210:240,
                           211:241,
                           212:242,
                           213:243,
                           214:244,
                           215:245,
                           216:246,
                           217:247,
                           218:248},
    "k8s_persistentvolume" : {1: {PRIMARY_KEY},
                              2:100},
    "k8s_persistentvolumeclaim" : {1: {PRIMARY_KEY},
                                   2:100},
    "k8s_hpa" : {1: {PRIMARY_KEY},
                 2:102,
                 3:103,
                 4:104,
                 5:105},
    "k8s_state" : {3: {LIMITED},
                   11: {LIMITED},
                   12:112},
    "mesos_pair" : {1: {PRIMARY_KEY}},
    "mesos_common" : {1: {PRIMARY_KEY}},
    "mesos_task" : {1: {PRIMARY_KEY}},
    "marathon_app" : {1: {PRIMARY_KEY}},
    "marathon_group" : {1: {PRIMARY_KEY}},
    "mesos_framework" : {1: {PRIMARY_KEY}},
    "mesos_slave" : {1: {PRIMARY_KEY}},
    "swarm_pair" : {1: {PRIMARY_KEY},
                    2: {PRIMARY_KEY}},
    "swarm_common" : {1: {PRIMARY_KEY}},
    "swarm_port" : {1: {PRIMARY_KEY},
                    2: {PRIMARY_KEY},
                    3: {PRIMARY_KEY}},
    "swarm_service" : {1: {PRIMARY_KEY},
                       5:7,
                       6:8},
    "swarm_task" : {1: {PRIMARY_KEY}},
    "swarm_node" : {1: {PRIMARY_KEY}},
    "swarm_state" : {5:6},
    "key_value" : {1: {PRIMARY_KEY},
                   2: {PRIMARY_KEY}},
    "agent_event" : {1: {PRIMARY_KEY},
                     2: {PRIMARY_KEY},
                     3: {PRIMARY_KEY},
                     4: {PRIMARY_KEY},
                     5: {PRIMARY_KEY}},
    "metrics" : {10:17,
                 7: {LIMITED},
                 9: {LIMITED},
                 18: {LIMITED},
                 19: {LIMITED},
                 22: {LIMITED},
                 26: {LIMITED},
                 41: {LIMITED},
                 42: {LIMITED}},
    "pod_status_count" : {1: {PRIMARY_KEY},
                          2:100},
    "prom_sample" : {1: {PRIMARY_KEY},
                     2:102,
                     4: {PRIMARY_KEY, UNIQUE}},
}

# This set describes types which we will skip. We won't generate any aggregation
# code for messages or fields of this type
skip = set([
    "aggregations64",
    "aggregations64_repeated",
    "aggregations_double",
    "matchlist_detail"
])

