/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bactext.c
 * Original Author:  linzhixian, 2015-3-16
 *
 * BACnet Text
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "bacnet/bactext.h"
#include "bacnet/indtext.h"
#include "bacnet/bacenum.h"

static const char *ASHRAE_Reserved_String = "Reserved for Use by ASHRAE";
static const char *Vendor_Proprietary_String = "Vendor Proprietary Value";

INDTEXT_DATA bacnet_object_type_names[] = {
    {OBJECT_ANALOG_INPUT, "Analog Input"}
    ,
    {OBJECT_ANALOG_OUTPUT, "Analog Output"}
    ,
    {OBJECT_ANALOG_VALUE, "Analog Value"}
    ,
    {OBJECT_BINARY_INPUT, "Binary Input"}
    ,
    {OBJECT_BINARY_OUTPUT, "Binary Output"}
    ,
    {OBJECT_BINARY_VALUE, "Binary Value"}
    ,
    {OBJECT_CALENDAR, "Calendar"}
    ,
    {OBJECT_COMMAND, "Command"}
    ,
    {OBJECT_DEVICE, "Device"}
    ,
    {OBJECT_EVENT_ENROLLMENT, "Event Enrollment"}
    ,
    {OBJECT_FILE, "File"}
    ,
    {OBJECT_GROUP, "Group"}
    ,
    {OBJECT_LOOP, "Loop"}
    ,
    {OBJECT_MULTI_STATE_INPUT, "Multi-state Input"}
    ,
    {OBJECT_MULTI_STATE_OUTPUT, "Multi-state Output"}
    ,
    {OBJECT_NOTIFICATION_CLASS, "Notification Class"}
    ,
    {OBJECT_PROGRAM, "Program"}
    ,
    {OBJECT_SCHEDULE, "Schedule"}
    ,
    {OBJECT_AVERAGING, "Averaging"}
    ,
    {OBJECT_MULTI_STATE_VALUE, "Multi-state Value"}
    ,
    {OBJECT_TRENDLOG, "TrendLog"}
    ,
    {OBJECT_LIFE_SAFETY_POINT, "Life Safety Point"}
    ,
    {OBJECT_LIFE_SAFETY_ZONE, "Life Safety Zone"}
    ,
    {OBJECT_ACCUMULATOR, "Accumulator"}
    ,
    {OBJECT_PULSE_CONVERTER, "Pulse Converter"}
    ,
    {OBJECT_EVENT_LOG, "Event Log"}
    ,
    {OBJECT_GLOBAL_GROUP, "Global Group"}
    ,
    {OBJECT_TREND_LOG_MULTIPLE, "Trend Log Multiple"}
    ,
    {OBJECT_LOAD_CONTROL, "Load Control"}
    ,
    {OBJECT_STRUCTURED_VIEW, "Structured View"}
    ,
    {OBJECT_ACCESS_DOOR, "Access Door"}
    ,
    {OBJECT_LIGHTING_OUTPUT, "Lighting Output"}
    ,
    {OBJECT_ACCESS_CREDENTIAL, "Access Credential"}
    ,
    {OBJECT_ACCESS_POINT, "Access Point"}
    ,
    {OBJECT_ACCESS_RIGHTS, "Access Rights"}
    ,
    {OBJECT_ACCESS_USER, "Access User"}
    ,
    {OBJECT_ACCESS_ZONE, "Access Zone"}
    ,
    {OBJECT_CREDENTIAL_DATA_INPUT, "Credential Data Input"}
    ,
    {OBJECT_NETWORK_SECURITY, "Network Security"}
    ,
    {OBJECT_BITSTRING_VALUE, "BitString Value"}
    ,
    {OBJECT_CHARACTERSTRING_VALUE, "CharacterString Value"}
    ,
    {OBJECT_DATE_PATTERN_VALUE, "Date Pattern Value"}
    ,
    {OBJECT_DATE_VALUE, "Date Value"}
    ,
    {OBJECT_DATETIME_PATTERN_VALUE, "DateTime Pattern Value"}
    ,
    {OBJECT_DATETIME_VALUE, "DateTime Value"}
    ,
    {OBJECT_INTEGER_VALUE, "Integer Value"}
    ,
    {OBJECT_LARGE_ANALOG_VALUE, "Large Analog Value"}
    ,
    {OBJECT_OCTETSTRING_VALUE, "OctetString Value"}
    ,
    {OBJECT_POSITIVE_INTEGER_VALUE, "Positive Integer Value"}
    ,
    {OBJECT_TIME_PATTERN_VALUE, "Time Pattern Value"}
    ,
    {OBJECT_TIME_VALUE, "Time Value"}
    ,
    {OBJECT_NOTIFICATION_FORWARDER, "Notification Forwarder"}
    ,
    {OBJECT_ALERT_ENROLLMENT, "Alert Enrollment"}
    ,
    {OBJECT_CHANNEL, "Channel"}
    ,
    {OBJECT_LIGHTING_OUTPUT, "Lighting Output"}
    ,
    {0, NULL}
    /* Enumerated values 0-127 are reserved for definition by ASHRAE.
       Enumerated values 128-1023 may be used by others subject to
       the procedures and constraints described in Clause 23. */
};

INDTEXT_DATA bacnet_property_names[] = {
/* FIXME: use the enumerations from bacenum.h */
    {PROP_ACKED_TRANSITIONS, "acked-transitions"}
    ,
    {PROP_ACK_REQUIRED, "ack-required"}
    ,
    {PROP_ACTION, "action"}
    ,
    {PROP_ACTION_TEXT, "action-text"}
    ,
    {PROP_ACTIVE_TEXT, "active-text"}
    ,
    {PROP_ACTIVE_VT_SESSIONS, "active-vt-sessions"}
    ,
    {PROP_ALARM_VALUE, "alarm-value"}
    ,
    {PROP_ALARM_VALUES, "alarm-values"}
    ,
    {PROP_ALL, "all"}
    ,
    {PROP_ALL_WRITES_SUCCESSFUL, "all-writes-successful"}
    ,
    {PROP_APDU_SEGMENT_TIMEOUT, "apdu-segment-timeout"}
    ,
    {PROP_APDU_TIMEOUT, "apdu-timeout"}
    ,
    {PROP_APPLICATION_SOFTWARE_VERSION, "application-software-version"}
    ,
    {PROP_ARCHIVE, "archive"}
    ,
    {PROP_BIAS, "bias"}
    ,
    {PROP_CHANGE_OF_STATE_COUNT, "change-of-state-count"}
    ,
    {PROP_CHANGE_OF_STATE_TIME, "change-of-state-time"}
    ,
    {PROP_NOTIFICATION_CLASS, "notification-class"}
    ,
    {PROP_BLANK_1, "(deleted in 135-2001)"}
    ,
    {PROP_CONTROLLED_VARIABLE_REFERENCE, "controlled-variable-reference"}
    ,
    {PROP_CONTROLLED_VARIABLE_UNITS, "controlled-variable-units"}
    ,
    {PROP_CONTROLLED_VARIABLE_VALUE, "controlled-variable-value"}
    ,
    {PROP_COV_INCREMENT, "COV-increment"}
    ,
    {PROP_DATE_LIST, "datelist"}
    ,
    {PROP_DAYLIGHT_SAVINGS_STATUS, "daylight-savings-status"}
    ,
    {PROP_DEADBAND, "deadband"}
    ,
    {PROP_DERIVATIVE_CONSTANT, "derivative-constant"}
    ,
    {PROP_DERIVATIVE_CONSTANT_UNITS, "derivative-constant-units"}
    ,
    {PROP_DESCRIPTION, "description"}
    ,
    {PROP_DESCRIPTION_OF_HALT, "description-of-halt"}
    ,
    {PROP_DEVICE_ADDRESS_BINDING, "device-address-binding"}
    ,
    {PROP_DEVICE_TYPE, "device-type"}
    ,
    {PROP_EFFECTIVE_PERIOD, "effective-period"}
    ,
    {PROP_ELAPSED_ACTIVE_TIME, "elapsed-active-time"}
    ,
    {PROP_ERROR_LIMIT, "error-limit"}
    ,
    {PROP_EVENT_ENABLE, "event-enable"}
    ,
    {PROP_EVENT_STATE, "event-state"}
    ,
    {PROP_EVENT_TYPE, "event-type"}
    ,
    {PROP_EXCEPTION_SCHEDULE, "exception-schedule"}
    ,
    {PROP_FAULT_VALUES, "fault-values"}
    ,
    {PROP_FEEDBACK_VALUE, "feedback-value"}
    ,
    {PROP_FILE_ACCESS_METHOD, "file-access-method"}
    ,
    {PROP_FILE_SIZE, "file-size"}
    ,
    {PROP_FILE_TYPE, "file-type"}
    ,
    {PROP_FIRMWARE_REVISION, "firmware-revision"}
    ,   /* VTS wants "revision", not "version" */
    {PROP_HIGH_LIMIT, "high-limit"}
    ,
    {PROP_INACTIVE_TEXT, "inactive-text"}
    ,
    {PROP_IN_PROCESS, "in-process"}
    ,
    {PROP_INSTANCE_OF, "instance-of"}
    ,
    {PROP_INTEGRAL_CONSTANT, "integral-constant"}
    ,
    {PROP_INTEGRAL_CONSTANT_UNITS, "integral-constant-units"}
    ,
    {PROP_ISSUE_CONFIRMED_NOTIFICATIONS, "issue-confirmednotifications"}
    ,
    {PROP_LIMIT_ENABLE, "limit-enable"}
    ,
    {PROP_LIST_OF_GROUP_MEMBERS, "list-of-group-members"}
    ,
    {PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES,
        "list-of-object-property-references"}
    ,
    {PROP_LIST_OF_SESSION_KEYS, "list-of-session-keys"}
    ,
    {PROP_LOCAL_DATE, "local-date"}
    ,
    {PROP_LOCAL_TIME, "local-time"}
    ,
    {PROP_LOCATION, "location"}
    ,
    {PROP_LOW_LIMIT, "low-limit"}
    ,
    {PROP_MANIPULATED_VARIABLE_REFERENCE, "manipulated-variable-reference"}
    ,
    {PROP_MAXIMUM_OUTPUT, "maximum-output"}
    ,
    {PROP_MAX_APDU_LENGTH_ACCEPTED, "max-apdu-length-accepted"}
    ,
    {PROP_MAX_INFO_FRAMES, "max-info-frames"}
    ,
    {PROP_MAX_MASTER, "max-master"}
    ,
    {PROP_MAX_PRES_VALUE, "max-pres-value"}
    ,
    {PROP_MINIMUM_OFF_TIME, "minimum-off-time"}
    ,
    {PROP_MINIMUM_ON_TIME, "minimum-on-time"}
    ,
    {PROP_MINIMUM_OUTPUT, "minimum-output"}
    ,
    {PROP_MIN_PRES_VALUE, "min-pres-value"}
    ,
    {PROP_MODEL_NAME, "model-name"}
    ,
    {PROP_MODIFICATION_DATE, "modification-date"}
    ,
    {PROP_NOTIFY_TYPE, "notify-type"}
    ,
    {PROP_NUMBER_OF_APDU_RETRIES, "number-of-APDU-retries"}
    ,
    {PROP_NUMBER_OF_STATES, "number-of-states"}
    ,
    {PROP_OBJECT_IDENTIFIER, "object-identifier"}
    ,
    {PROP_OBJECT_LIST, "object-list"}
    ,
    {PROP_OBJECT_NAME, "object-name"}
    ,
    {PROP_OBJECT_PROPERTY_REFERENCE, "object-property-reference"}
    ,
    {PROP_OBJECT_TYPE, "object-type"}
    ,
    {PROP_OPTIONAL, "optional"}
    ,
    {PROP_OUT_OF_SERVICE, "out-of-service"}
    ,
    {PROP_OUTPUT_UNITS, "output-units"}
    ,
    {PROP_EVENT_PARAMETERS, "event-parameters"}
    ,
    {PROP_POLARITY, "polarity"}
    ,
    {PROP_PRESENT_VALUE, "present-value"}
    ,
    {PROP_PRIORITY, "priority"}
    ,
    {PROP_PRIORITY_ARRAY, "priority-array"}
    ,
    {PROP_PRIORITY_FOR_WRITING, "priority-for-writing"}
    ,
    {PROP_PROCESS_IDENTIFIER, "process-identifier"}
    ,
    {PROP_PROGRAM_CHANGE, "program-change"}
    ,
    {PROP_PROGRAM_LOCATION, "program-location"}
    ,
    {PROP_PROGRAM_STATE, "program-state"}
    ,
    {PROP_PROPORTIONAL_CONSTANT, "proportional-constant"}
    ,
    {PROP_PROPORTIONAL_CONSTANT_UNITS, "proportional-constant-units"}
    ,
    {PROP_PROTOCOL_CONFORMANCE_CLASS, "protocol-conformance-class"}
    ,
    {PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED,
        "protocol-object-types-supported"}
    ,
    {PROP_PROTOCOL_SERVICES_SUPPORTED, "protocol-services-supported"}
    ,
    {PROP_PROTOCOL_VERSION, "protocol-version"}
    ,
    {PROP_READ_ONLY, "read-only"}
    ,
    {PROP_REASON_FOR_HALT, "reason-for-halt"}
    ,
    {PROP_RECIPIENT, "recipient"}
    ,
    {PROP_RECIPIENT_LIST, "recipient-list"}
    ,
    {PROP_RELIABILITY, "reliability"}
    ,
    {PROP_RELINQUISH_DEFAULT, "relinquish-default"}
    ,
    {PROP_REQUIRED, "required"}
    ,
    {PROP_RESOLUTION, "resolution"}
    ,
    {PROP_SEGMENTATION_SUPPORTED, "segmentation-supported"}
    ,
    {PROP_SETPOINT, "setpoint"}
    ,
    {PROP_SETPOINT_REFERENCE, "setpoint-reference"}
    ,
    {PROP_STATE_TEXT, "state-text"}
    ,
    {PROP_STATUS_FLAGS, "status-flags"}
    ,
    {PROP_SYSTEM_STATUS, "system-status"}
    ,
    {PROP_TIME_DELAY, "time-delay"}
    ,
    {PROP_TIME_OF_ACTIVE_TIME_RESET, "time-of-active-time-reset"}
    ,
    {PROP_TIME_OF_STATE_COUNT_RESET, "time-of-state-count-reset"}
    ,
    {PROP_TIME_SYNCHRONIZATION_RECIPIENTS,
        "time-synchronization-recipients"}
    ,
    {PROP_UNITS, "units"}
    ,
    {PROP_UPDATE_INTERVAL, "update-interval"}
    ,
    {PROP_UTC_OFFSET, "utc-offset"}
    ,
    {PROP_VENDOR_IDENTIFIER, "vendor-identifier"}
    ,
    {PROP_VENDOR_NAME, "vendor-name"}
    ,
    {PROP_VT_CLASSES_SUPPORTED, "vt-classes-supported"}
    ,
    {PROP_WEEKLY_SCHEDULE, "weekly-schedule"}
    ,
    {PROP_ATTEMPTED_SAMPLES, "attempted-samples"}
    ,
    {PROP_AVERAGE_VALUE, "average-value"}
    ,
    {PROP_BUFFER_SIZE, "buffer-size"}
    ,
    {PROP_CLIENT_COV_INCREMENT, "client-cov-increment"}
    ,
    {PROP_COV_RESUBSCRIPTION_INTERVAL, "cov-resubscription-interval"}
    ,
    {PROP_CURRENT_NOTIFY_TIME, "current-notify-time"}
    ,
    {PROP_EVENT_TIME_STAMPS, "event-time-stamps"}
    ,
    {PROP_LOG_BUFFER, "log-buffer"}
    ,
    {PROP_LOG_DEVICE_OBJECT_PROPERTY, "log-device-object-property"}
    ,
    {PROP_ENABLE, "enable"}
    ,
    {PROP_LOG_INTERVAL, "log-interval"}
    ,
    {PROP_MAXIMUM_VALUE, "maximum-value"}
    ,
    {PROP_MINIMUM_VALUE, "minimum-value"}
    ,
    {PROP_NOTIFICATION_THRESHOLD, "notification-threshold"}
    ,
    {PROP_PREVIOUS_NOTIFY_TIME, "previous-notify-time"}
    ,
    {PROP_PROTOCOL_REVISION, "protocol-revision"}
    ,
    {PROP_RECORDS_SINCE_NOTIFICATION, "records-since-notification"}
    ,
    {PROP_RECORD_COUNT, "record-count"}
    ,
    {PROP_START_TIME, "start-time"}
    ,
    {PROP_STOP_TIME, "stop-time"}
    ,
    {PROP_STOP_WHEN_FULL, "stop-when-full"}
    ,
    {PROP_TOTAL_RECORD_COUNT, "total-record-count"}
    ,
    {PROP_VALID_SAMPLES, "valid-samples"}
    ,
    {PROP_WINDOW_INTERVAL, "window-interval"}
    ,
    {PROP_WINDOW_SAMPLES, "window-samples"}
    ,
    {PROP_MAXIMUM_VALUE_TIMESTAMP, "maximum-value-timestamp"}
    ,
    {PROP_MINIMUM_VALUE_TIMESTAMP, "minimum-value-timestamp"}
    ,
    {PROP_VARIANCE_VALUE, "variance-value"}
    ,
    {PROP_ACTIVE_COV_SUBSCRIPTIONS, "active-cov-subscriptions"}
    ,
    {PROP_BACKUP_FAILURE_TIMEOUT, "backup-failure-timeout"}
    ,
    {PROP_CONFIGURATION_FILES, "configuration-files"}
    ,
    {PROP_DATABASE_REVISION, "database-revision"}
    ,
    {PROP_DIRECT_READING, "direct-reading"}
    ,
    {PROP_LAST_RESTORE_TIME, "last-restore-time"}
    ,
    {PROP_MAINTENANCE_REQUIRED, "maintenance-required"}
    ,
    {PROP_MEMBER_OF, "member-of"}
    ,
    {PROP_MODE, "mode"}
    ,
    {PROP_OPERATION_EXPECTED, "operation-expected"}
    ,
    {PROP_SETTING, "setting"}
    ,
    {PROP_SILENCED, "silenced"}
    ,
    {PROP_TRACKING_VALUE, "tracking-value"}
    ,
    {PROP_ZONE_MEMBERS, "zone-members"}
    ,
    {PROP_LIFE_SAFETY_ALARM_VALUES, "life-safety-alarm-values"}
    ,
    {PROP_MAX_SEGMENTS_ACCEPTED, "max-segments-accepted"}
    ,
    {PROP_PROFILE_NAME, "profile-name"}
    ,
    {PROP_AUTO_SLAVE_DISCOVERY, "auto-slave-discovery"}
    ,
    {PROP_MANUAL_SLAVE_ADDRESS_BINDING, "manual-slave-address-binding"}
    ,
    {PROP_SLAVE_ADDRESS_BINDING, "slave-address-binding"}
    ,
    {PROP_SLAVE_PROXY_ENABLE, "slave-proxy-enable"}
    ,
    {PROP_LAST_NOTIFY_RECORD, "last-notify-record"}
    ,
    {PROP_SCHEDULE_DEFAULT, "schedule-default"}
    ,
    {PROP_ACCEPTED_MODES, "accepted-modes"}
    ,
    {PROP_ADJUST_VALUE, "adjust-value"}
    ,
    {PROP_COUNT, "count"}
    ,
    {PROP_COUNT_BEFORE_CHANGE, "count-before-change"}
    ,
    {PROP_COUNT_CHANGE_TIME, "count-change-time"}
    ,
    {PROP_COV_PERIOD, "COV-period"}
    ,
    {PROP_INPUT_REFERENCE, "input-reference"}
    ,
    {PROP_LIMIT_MONITORING_INTERVAL, "limit-monitoring-interval"}
    ,
    {PROP_LOGGING_OBJECT, "logging-object"}
    ,
    {PROP_LOGGING_RECORD, "logging-record"}
    ,
    {PROP_PRESCALE, "prescale"}
    ,
    {PROP_PULSE_RATE, "pulse-rate"}
    ,
    {PROP_SCALE, "scale"}
    ,
    {PROP_SCALE_FACTOR, "scale-factor"}
    ,
    {PROP_UPDATE_TIME, "update-time"}
    ,
    {PROP_VALUE_BEFORE_CHANGE, "value-before-change"}
    ,
    {PROP_VALUE_SET, "value-set"}
    ,
    {PROP_VALUE_CHANGE_TIME, "value-change-time"}
    ,
    {PROP_ALIGN_INTERVALS, "align-intervals"}
    ,
    {PROP_INTERVAL_OFFSET, "interval-offset"}
    ,
    {PROP_LAST_RESTART_REASON, "last-restart-reason"}
    ,
    {PROP_LOGGING_TYPE, "logging-type"}
    ,
    {PROP_TIME_OF_DEVICE_RESTART, "time-of-device-restart"}
    ,
    {PROP_TIME_SYNCHRONIZATION_INTERVAL, "time-synchronization-interval"}
    ,
    {PROP_TRIGGER, "trigger"}
    ,
    {PROP_UTC_TIME_SYNCHRONIZATION_RECIPIENTS,
        "utc-time-synchronization-recipients"}
    ,
    {PROP_NODE_SUBTYPE, "node-subtype"}
    ,
    {PROP_NODE_TYPE, "node-type"}
    ,
    {PROP_STRUCTURED_OBJECT_LIST, "structured-object-list"}
    ,
    {PROP_SUBORDINATE_ANNOTATIONS, "subordinate-annotations"}
    ,
    {PROP_SUBORDINATE_LIST, "subordinate-list"}
    ,
    {PROP_ACTUAL_SHED_LEVEL, "actual-shed-level"}
    ,
    {PROP_DUTY_WINDOW, "duty-window"}
    ,
    {PROP_EXPECTED_SHED_LEVEL, "expected-shed-level"}
    ,
    {PROP_FULL_DUTY_BASELINE, "full-duty-baseline"}
    ,
    {PROP_REQUESTED_SHED_LEVEL, "requested-shed-level"}
    ,
    {PROP_SHED_DURATION, "shed-duration"}
    ,
    {PROP_SHED_LEVEL_DESCRIPTIONS, "shed-level-descriptions"}
    ,
    {PROP_SHED_LEVELS, "shed-levels"}
    ,
    {PROP_STATE_DESCRIPTION, "state-description"}
    ,
    {PROP_DOOR_ALARM_STATE, "door-alarm-state"}
    ,
    {PROP_DOOR_EXTENDED_PULSE_TIME, "door-extended-pulse-time"}
    ,
    {PROP_DOOR_MEMBERS, "door-members"}
    ,
    {PROP_DOOR_OPEN_TOO_LONG_TIME, "door-open-too-long-time"}
    ,
    {PROP_DOOR_PULSE_TIME, "door-pulse-time"}
    ,
    {PROP_DOOR_STATUS, "door-status"}
    ,
    {PROP_DOOR_UNLOCK_DELAY_TIME, "door-unlock-delay-time"}
    ,
    {PROP_LOCK_STATUS, "lock-status"}
    ,
    {PROP_MASKED_ALARM_VALUES, "masked-alarm-values"}
    ,
    {PROP_SECURED_STATUS, "secured-status"}
    ,
    {PROP_ABSENTEE_LIMIT, "absentee-limit"}
    ,
    {PROP_ACCESS_ALARM_EVENTS, "access-alarm-events"}
    ,
    {PROP_ACCESS_DOORS, "access-doors"}
    ,
    {PROP_ACCESS_EVENT, "access-event"}
    ,
    {PROP_ACCESS_EVENT_AUTHENTICATION_FACTOR,
        "access-event-authentication-factor"}
    ,
    {PROP_ACCESS_EVENT_CREDENTIAL, "access-event-credential"}
    ,
    {PROP_ACCESS_EVENT_TIME, "access-event-time"}
    ,
    {PROP_ACCESS_TRANSACTION_EVENTS, "access-transaction-events"}
    ,
    {PROP_ACCOMPANIMENT, "accompaniment"}
    ,
    {PROP_ACCOMPANIMENT_TIME, "accompaniment-time"}
    ,
    {PROP_ACTIVATION_TIME, "activation-time"}
    ,
    {PROP_ACTIVE_AUTHENTICATION_POLICY, "active-authentication-policy"}
    ,
    {PROP_ASSIGNED_ACCESS_RIGHTS, "assigned-access-rights"}
    ,
    {PROP_AUTHENTICATION_FACTORS, "authentication-factors"}
    ,
    {PROP_AUTHENTICATION_POLICY_LIST, "authentication-policy-list"}
    ,
    {PROP_AUTHENTICATION_POLICY_NAMES, "authentication-policy-names"}
    ,
    {PROP_AUTHORIZATION_STATUS, "authentication-status"}
    ,
    {PROP_AUTHORIZATION_MODE, "authorization-mode"}
    ,
    {PROP_BELONGS_TO, "belongs-to"}
    ,
    {PROP_CREDENTIAL_DISABLE, "credential-disable"}
    ,
    {PROP_CREDENTIAL_STATUS, "credential-status"}
    ,
    {PROP_CREDENTIALS, "credentials"}
    ,
    {PROP_CREDENTIALS_IN_ZONE, "credentials-in-zone"}
    ,
    {PROP_DAYS_REMAINING, "days-remaining"}
    ,
    {PROP_ENTRY_POINTS, "entry-points"}
    ,
    {PROP_EXIT_POINTS, "exit-points"}
    ,
    {PROP_EXPIRY_TIME, "expiry-time"}
    ,
    {PROP_EXTENDED_TIME_ENABLE, "extended-time-enable"}
    ,
    {PROP_FAILED_ATTEMPT_EVENTS, "failed-attempt-events"}
    ,
    {PROP_FAILED_ATTEMPTS, "failed-attempts"}
    ,
    {PROP_FAILED_ATTEMPTS_TIME, "failed-attempts-time"}
    ,
    {PROP_LAST_ACCESS_EVENT, "last-access-event"}
    ,
    {PROP_LAST_ACCESS_POINT, "last-access-point"}
    ,
    {PROP_LAST_CREDENTIAL_ADDED, "last-credential-added"}
    ,
    {PROP_LAST_CREDENTIAL_ADDED_TIME, "last-credential-added-time"}
    ,
    {PROP_LAST_CREDENTIAL_REMOVED, "last-credential-removed"}
    ,
    {PROP_LAST_CREDENTIAL_REMOVED_TIME, "last-credential-removed-time"}
    ,
    {PROP_LAST_USE_TIME, "last-use-time"}
    ,
    {PROP_LOCKOUT, "lockout"}
    ,
    {PROP_LOCKOUT_RELINQUISH_TIME, "lockout-relinquish-time"}
    ,
    {PROP_MASTER_EXEMPTION, "master-exemption"}
    ,
    {PROP_MAX_FAILED_ATTEMPTS, "max-failed-attempts"}
    ,
    {PROP_MEMBERS, "members"}
    ,
    {PROP_MUSTER_POINT, "muster-point"}
    ,
    {PROP_NEGATIVE_ACCESS_RULES, "negative-access-rules"}
    ,
    {PROP_NUMBER_OF_AUTHENTICATION_POLICIES,
        "number-of-authentication-policies"}
    ,
    {PROP_OCCUPANCY_COUNT, "occupancy-count"}
    ,
    {PROP_OCCUPANCY_COUNT_ADJUST, "occupancy-count-adjust"}
    ,
    {PROP_OCCUPANCY_COUNT_ENABLE, "occupancy-count-enable"}
    ,
    {PROP_OCCUPANCY_EXEMPTION, "occupancy-exemption"}
    ,
    {PROP_OCCUPANCY_LOWER_LIMIT, "occupancy-lower-limit"}
    ,
    {PROP_OCCUPANCY_LOWER_LIMIT_ENFORCED,
        "occupancy-lower-limit-enforced"}
    ,
    {PROP_OCCUPANCY_STATE, "occupancy-state"}
    ,
    {PROP_OCCUPANCY_UPPER_LIMIT, "occupancy-upper-limit"}
    ,
    {PROP_OCCUPANCY_UPPER_LIMIT_ENFORCED, "occupancy-upper-limit-enforced"}
    ,
    {PROP_PASSBACK_EXEMPTION, "passback-exemption"}
    ,
    {PROP_PASSBACK_MODE, "passback-mode"}
    ,
    {PROP_PASSBACK_TIMEOUT, "passback-timeout"}
    ,
    {PROP_POSITIVE_ACCESS_RULES, "positive-access-rules"}
    ,
    {PROP_REASON_FOR_DISABLE, "reason-for-disable"}
    ,
    {PROP_SUPPORTED_FORMATS, "supported-formats"}
    ,
    {PROP_SUPPORTED_FORMAT_CLASSES, "supported-format-classes"}
    ,
    {PROP_THREAT_AUTHORITY, "threat-authority"}
    ,
    {PROP_THREAT_LEVEL, "threat-level"}
    ,
    {PROP_TRACE_FLAG, "trace-flag"}
    ,
    {PROP_TRANSACTION_NOTIFICATION_CLASS, "transaction-notification-class"}
    ,
    {PROP_USER_EXTERNAL_IDENTIFIER, "user-external-identifier"}
    ,
    {PROP_USER_INFORMATION_REFERENCE, "user-information-reference"}
    ,
    {PROP_USER_INFORMATION_REFERENCE, "user-information-reference"}
    ,
    {PROP_USER_NAME, "user-name"}
    ,
    {PROP_USER_TYPE, "user-type"}
    ,
    {PROP_USES_REMAINING, "uses-remaining"}
    ,
    {PROP_ZONE_FROM, "zone-from"}
    ,
    {PROP_ZONE_TO, "zone-to"}
    ,
    {PROP_VERIFICATION_TIME, "verification-time"}
    ,
    {PROP_BASE_DEVICE_SECURITY_POLICY, "base-device-security-policy"}
    ,
    {PROP_DISTRIBUTION_KEY_REVISION, "distribution-key-revision"}
    ,
    {PROP_DO_NOT_HIDE, "do-not-hide"}
    ,
    {PROP_KEY_SETS, "key-sets"}
    ,
    {PROP_LAST_KEY_SERVER, "last-key-server"}
    ,
    {PROP_NETWORK_ACCESS_SECURITY_POLICIES,
        "network-access-security-policies"}
    ,
    {PROP_PACKET_REORDER_TIME, "packet-reorder-time"}
    ,
    {PROP_SECURITY_PDU_TIMEOUT, "security-pdu-timeout"}
    ,
    {PROP_SECURITY_TIME_WINDOW, "security-time-window"}
    ,
    {PROP_SUPPORTED_SECURITY_ALGORITHM, "supported-security-algorithm"}
    ,
    {PROP_UPDATE_KEY_SET_TIMEOUT, "update-key-set-timeout"}
    ,
    {PROP_BACKUP_AND_RESTORE_STATE, "backup-and-restore-state"}
    ,
    {PROP_BACKUP_PREPARATION_TIME, "backup-preparation-time"}
    ,
    {PROP_RESTORE_COMPLETION_TIME, "restore-completion-time"}
    ,
    {PROP_RESTORE_PREPARATION_TIME, "restore-preparation-time"}
    ,
    {PROP_BIT_MASK, "bit-mask"}
    ,
    {PROP_BIT_TEXT, "bit-text"}
    ,
    {PROP_IS_UTC, "is-utc"}
    ,
    {PROP_GROUP_MEMBERS, "group-members"}
    ,
    {PROP_GROUP_MEMBER_NAMES, "group-member-names"}
    ,
    {PROP_MEMBER_STATUS_FLAGS, "member-status-flags"}
    ,
    {PROP_REQUESTED_UPDATE_INTERVAL, "requested-update-interval"}
    ,
    {PROP_COVU_PERIOD, "covu-period"}
    ,
    {PROP_COVU_RECIPIENTS, "covu-recipients"}
    ,
    {PROP_EVENT_MESSAGE_TEXTS, "event-message-texts"}
    ,
    {PROP_EVENT_MESSAGE_TEXTS_CONFIG, "event-message-texts-config"}
    ,
    {PROP_EVENT_DETECTION_ENABLE, "event-detection-enable"}
    ,
    {PROP_EVENT_ALGORITHM_INHIBIT, "event-algorithm-inhibit"}
    ,
    {PROP_EVENT_ALGORITHM_INHIBIT_REF, "event-algorithm-inhibit-ref"}
    ,
    {PROP_TIME_DELAY_NORMAL, "time-delay-normal"}
    ,
    {PROP_RELIABILITY_EVALUATION_INHIBIT, "reliability-evaluation-inhibit"}
    ,
    {PROP_FAULT_PARAMETERS, "fault-parameters"}
    ,
    {PROP_FAULT_TYPE, "fault-type"}
    ,
    {PROP_LOCAL_FORWARDING_ONLY, "local-forwarding-only"}
    ,
    {PROP_PROCESS_IDENTIFIER_FILTER, "process-identifier-filter"}
    ,
    {PROP_SUBSCRIBED_RECIPIENTS, "subscribed-recipients"}
    ,
    {PROP_PORT_FILTER, "port-filter"}
    ,
    {PROP_AUTHORIZATION_EXEMPTIONS, "authorization-exemptions"}
    ,
    {PROP_ALLOW_GROUP_DELAY_INHIBIT, "allow-group-delay-inhibit"}
    ,
    {PROP_CHANNEL_NUMBER, "channel-number"}
    ,
    {PROP_CONTROL_GROUPS, "control-groups"}
    ,
    {PROP_EXECUTION_DELAY, "execution-delay"}
    ,
    {PROP_LAST_PRIORITY, "last-priority"}
    ,
    {PROP_WRITE_STATUS, "write-status"}
    ,
    {PROP_PROPERTY_LIST, "property-list"}
    ,
    {PROP_SERIAL_NUMBER, "serial-number"}
    ,
    {PROP_BLINK_WARN_ENABLE, "blink-warn-enable"}
    ,
    {PROP_DEFAULT_FADE_TIME, "default-fade-time"}
    ,
    {PROP_DEFAULT_RAMP_RATE, "default-ramp-rate"}
    ,
    {PROP_DEFAULT_STEP_INCREMENT, "default-step-increment"}
    ,
    {PROP_EGRESS_TIMER, "egress-timer"}
    ,
    {PROP_IN_PROGRESS, "in-progress"}
    ,
    {PROP_INSTANTANEOUS_POWER, "instantaneous-power"}
    ,
    {PROP_LIGHTING_COMMAND, "lighting-command"}
    ,
    {PROP_LIGHTING_COMMAND_DEFAULT_PRIORITY,
            "lighting-command-default-priority"}
    ,
    {PROP_MAX_ACTUAL_VALUE, "max-actual-value"}
    ,
    {PROP_MIN_ACTUAL_VALUE, "min-actual-value"}
    ,
    {PROP_POWER, "power"}
    ,
    {PROP_TRANSITION, "transition"}
    ,
    {PROP_EGRESS_ACTIVE, "egress-active"}
    ,
    {0, NULL}
    /* Enumerated values 0-511 are reserved for definition by ASHRAE.
       Enumerated values 512-4194303 may be used by others subject to the
       procedures and constraints described in Clause 23. */
};

INDTEXT_DATA bacnet_event_state_names[] = {
    {EVENT_STATE_NORMAL, "normal"}
    ,
    {EVENT_STATE_FAULT, "fault"}
    ,
    {EVENT_STATE_OFFNORMAL, "offnormal"}
    ,
    {EVENT_STATE_HIGH_LIMIT, "high limit"}
    ,
    {EVENT_STATE_LOW_LIMIT, "low limit"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_engineering_unit_names[] = {
    {UNITS_SQUARE_METERS, "square-meters"}
    ,
    {UNITS_SQUARE_FEET, "square-feet"}
    ,
    {UNITS_MILLIAMPERES, "milliamperes"}
    ,
    {UNITS_AMPERES, "amperes"}
    ,
    {UNITS_OHMS, "ohms"}
    ,
    {UNITS_VOLTS, "volts"}
    ,
    {UNITS_KILOVOLTS, "kilovolts"}
    ,
    {UNITS_MEGAVOLTS, "megavolts"}
    ,
    {UNITS_VOLT_AMPERES, "volt-amperes"}
    ,
    {UNITS_KILOVOLT_AMPERES, "kilovolt-amperes"}
    ,
    {UNITS_MEGAVOLT_AMPERES, "megavolt-amperes"}
    ,
    {UNITS_VOLT_AMPERES_REACTIVE, "volt-amperes-reactive"}
    ,
    {UNITS_KILOVOLT_AMPERES_REACTIVE, "kilovolt-amperes-reactive"}
    ,
    {UNITS_MEGAVOLT_AMPERES_REACTIVE, "megavolt-amperes-reactive"}
    ,
    {UNITS_DEGREES_PHASE, "degrees-phase"}
    ,
    {UNITS_POWER_FACTOR, "power-factor"}
    ,
    {UNITS_JOULES, "joules"}
    ,
    {UNITS_KILOJOULES, "kilojoules"}
    ,
    {UNITS_WATT_HOURS, "watt-hours"}
    ,
    {UNITS_KILOWATT_HOURS, "kilowatt-hours"}
    ,
    {UNITS_BTUS, "btus"}
    ,
    {UNITS_THERMS, "therms"}
    ,
    {UNITS_TON_HOURS, "ton-hours"}
    ,
    {UNITS_JOULES_PER_KILOGRAM_DRY_AIR, "joules-per-kilogram-dry-air"}
    ,
    {UNITS_BTUS_PER_POUND_DRY_AIR, "btus-per-pound-dry-air"}
    ,
    {UNITS_CYCLES_PER_HOUR, "cycles-per-hour"}
    ,
    {UNITS_CYCLES_PER_MINUTE, "cycles-per-minute"}
    ,
    {UNITS_HERTZ, "hertz"}
    ,
    {UNITS_GRAMS_OF_WATER_PER_KILOGRAM_DRY_AIR,
        "grams-of-water-per-kilogram-dry-air"}
    ,
    {UNITS_PERCENT_RELATIVE_HUMIDITY, "percent-relative-humidity"}
    ,
    {UNITS_MILLIMETERS, "millimeters"}
    ,
    {UNITS_METERS, "meters"}
    ,
    {UNITS_INCHES, "inches"}
    ,
    {UNITS_FEET, "feet"}
    ,
    {UNITS_WATTS_PER_SQUARE_FOOT, "watts-per-square-foot"}
    ,
    {UNITS_WATTS_PER_SQUARE_METER, "watts-per-square-meter"}
    ,
    {UNITS_LUMENS, "lumens"}
    ,
    {UNITS_LUXES, "luxes"}
    ,
    {UNITS_FOOT_CANDLES, "foot-candles"}
    ,
    {UNITS_KILOGRAMS, "kilograms"}
    ,
    {UNITS_POUNDS_MASS, "pounds-mass"}
    ,
    {UNITS_TONS, "tons"}
    ,
    {UNITS_KILOGRAMS_PER_SECOND, "kilograms-per-second"}
    ,
    {UNITS_KILOGRAMS_PER_MINUTE, "kilograms-per-minute"}
    ,
    {UNITS_KILOGRAMS_PER_HOUR, "kilograms-per-hour"}
    ,
    {UNITS_POUNDS_MASS_PER_MINUTE, "pounds-mass-per-minute"}
    ,
    {UNITS_POUNDS_MASS_PER_HOUR, "pounds-mass-per-hour"}
    ,
    {UNITS_WATTS, "watts"}
    ,
    {UNITS_KILOWATTS, "kilowatts"}
    ,
    {UNITS_MEGAWATTS, "megawatts"}
    ,
    {UNITS_BTUS_PER_HOUR, "btus-per-hour"}
    ,
    {UNITS_HORSEPOWER, "horsepower"}
    ,
    {UNITS_TONS_REFRIGERATION, "tons-refrigeration"}
    ,
    {UNITS_PASCALS, "pascals"}
    ,
    {UNITS_KILOPASCALS, "kilopascals"}
    ,
    {UNITS_BARS, "bars"}
    ,
    {UNITS_POUNDS_FORCE_PER_SQUARE_INCH, "pounds-force-per-square-inch"}
    ,
    {UNITS_CENTIMETERS_OF_WATER, "centimeters-of-water"}
    ,
    {UNITS_INCHES_OF_WATER, "inches-of-water"}
    ,
    {UNITS_MILLIMETERS_OF_MERCURY, "millimeters-of-mercury"}
    ,
    {UNITS_CENTIMETERS_OF_MERCURY, "centimeters-of-mercury"}
    ,
    {UNITS_INCHES_OF_MERCURY, "inches-of-mercury"}
    ,
    {UNITS_DEGREES_CELSIUS, "degrees-celsius"}
    ,
    {UNITS_DEGREES_KELVIN, "degrees-kelvin"}
    ,
    {UNITS_DEGREES_FAHRENHEIT, "degrees-fahrenheit"}
    ,
    {UNITS_DEGREE_DAYS_CELSIUS, "degree-days-celsius"}
    ,
    {UNITS_DEGREE_DAYS_FAHRENHEIT, "degree-days-fahrenheit"}
    ,
    {UNITS_YEARS, "years"}
    ,
    {UNITS_MONTHS, "months"}
    ,
    {UNITS_WEEKS, "weeks"}
    ,
    {UNITS_DAYS, "days"}
    ,
    {UNITS_HOURS, "hours"}
    ,
    {UNITS_MINUTES, "minutes"}
    ,
    {UNITS_SECONDS, "seconds"}
    ,
    {UNITS_METERS_PER_SECOND, "meters-per-second"}
    ,
    {UNITS_KILOMETERS_PER_HOUR, "kilometers-per-hour"}
    ,
    {UNITS_FEET_PER_SECOND, "feet-per-second"}
    ,
    {UNITS_FEET_PER_MINUTE, "feet-per-minute"}
    ,
    {UNITS_MILES_PER_HOUR, "miles-per-hour"}
    ,
    {UNITS_CUBIC_FEET, "cubic-feet"}
    ,
    {UNITS_CUBIC_METERS, "cubic-meters"}
    ,
    {UNITS_IMPERIAL_GALLONS, "imperial-gallons"}
    ,
    {UNITS_LITERS, "liters"}
    ,
    {UNITS_US_GALLONS, "us-gallons"}
    ,
    {UNITS_CUBIC_FEET_PER_MINUTE, "cubic-feet-per-minute"}
    ,
    {UNITS_CUBIC_METERS_PER_SECOND, "cubic-meters-per-second"}
    ,
    {UNITS_IMPERIAL_GALLONS_PER_MINUTE, "imperial-gallons-per-minute"}
    ,
    {UNITS_LITERS_PER_SECOND, "liters-per-second"}
    ,
    {UNITS_LITERS_PER_MINUTE, "liters-per-minute"}
    ,
    {UNITS_US_GALLONS_PER_MINUTE, "us-gallons-per-minute"}
    ,
    {UNITS_DEGREES_ANGULAR, "degrees-angular"}
    ,
    {UNITS_DEGREES_CELSIUS_PER_HOUR, "degrees-celsius-per-hour"}
    ,
    {UNITS_DEGREES_CELSIUS_PER_MINUTE, "degrees-celsius-per-minute"}
    ,
    {UNITS_DEGREES_FAHRENHEIT_PER_HOUR, "degrees-fahrenheit-per-hour"}
    ,
    {UNITS_DEGREES_FAHRENHEIT_PER_MINUTE, "degrees-fahrenheit-per-minute"}
    ,
    {UNITS_NO_UNITS, "no-units"}
    ,
    {UNITS_PARTS_PER_MILLION, "parts-per-million"}
    ,
    {UNITS_PARTS_PER_BILLION, "parts-per-billion"}
    ,
    {UNITS_PERCENT, "percent"}
    ,
    {UNITS_PERCENT_PER_SECOND, "percent-per-second"}
    ,
    {UNITS_PER_MINUTE, "per-minute"}
    ,
    {UNITS_PER_SECOND, "per-second"}
    ,
    {UNITS_PSI_PER_DEGREE_FAHRENHEIT, "psi-per-degree-fahrenheit"}
    ,
    {UNITS_RADIANS, "radians"}
    ,
    {UNITS_REVOLUTIONS_PER_MINUTE, "revolutions-per-minute"}
    ,
    {UNITS_CURRENCY1, "currency1"}
    ,
    {UNITS_CURRENCY2, "currency2"}
    ,
    {UNITS_CURRENCY3, "currency3"}
    ,
    {UNITS_CURRENCY4, "currency4"}
    ,
    {UNITS_CURRENCY5, "currency5"}
    ,
    {UNITS_CURRENCY6, "currency6"}
    ,
    {UNITS_CURRENCY7, "currency7"}
    ,
    {UNITS_CURRENCY8, "currency8"}
    ,
    {UNITS_CURRENCY9, "currency9"}
    ,
    {UNITS_CURRENCY10, "currency10"}
    ,
    {UNITS_SQUARE_INCHES, "square-inches"}
    ,
    {UNITS_SQUARE_CENTIMETERS, "square-centimeters"}
    ,
    {UNITS_BTUS_PER_POUND, "btus_per-pound"}
    ,
    {UNITS_CENTIMETERS, "centimeters"}
    ,
    {UNITS_POUNDS_MASS_PER_SECOND, "pounds-mass-per-second"}
    ,
    {UNITS_DELTA_DEGREES_FAHRENHEIT, "delta-degrees-fahrenheit"}
    ,
    {UNITS_DELTA_DEGREES_KELVIN, "delta-degrees-kelvin"}
    ,
    {UNITS_KILOHMS, "kilohms"}
    ,
    {UNITS_MEGOHMS, "megohms"}
    ,
    {UNITS_MILLIVOLTS, "millivolts"}
    ,
    {UNITS_KILOJOULES_PER_KILOGRAM, "kilojoules-per-kilogram"}
    ,
    {UNITS_MEGAJOULES, "megajoules"}
    ,
    {UNITS_JOULES_PER_DEGREE_KELVIN, "joules-per-degree-kelvin"}
    ,
    {UNITS_JOULES_PER_KILOGRAM_DEGREE_KELVIN,
        "joules-per-kilogram-degree-kelvin"}
    ,
    {UNITS_KILOHERTZ, "kilohertz"}
    ,
    {UNITS_MEGAHERTZ, "megahertz"}
    ,
    {UNITS_PER_HOUR, "per-hour"}
    ,
    {UNITS_MILLIWATTS, "milliwatts"}
    ,
    {UNITS_HECTOPASCALS, "hectopascals"}
    ,
    {UNITS_MILLIBARS, "millibars"}
    ,
    {UNITS_CUBIC_METERS_PER_HOUR, "cubic-meters-per-hour"}
    ,
    {UNITS_LITERS_PER_HOUR, "liters-per-hour"}
    ,
    {UNITS_KW_HOURS_PER_SQUARE_METER,
        "kilowatt-hours-per-square-meter"}
    ,
    {UNITS_KW_HOURS_PER_SQUARE_FOOT, "kilowatt-hours-per-square-foot"}
    ,
    {UNITS_MEGAJOULES_PER_SQUARE_METER, "megajoules-per-square-meter"}
    ,
    {UNITS_MEGAJOULES_PER_SQUARE_FOOT, "megajoules-per-square-foot"}
    ,
    {UNITS_CUBIC_FEET_PER_SECOND, "cubic-feet-per-second"}
    ,
    {UNITS_WATTS_PER_SQUARE_METER_DEGREE_KELVIN,
        "watts-per-square-meter-degree-kelvin"}
    ,
    {UNITS_PERCENT_OBSCURATION_PER_FOOT, "percent-obscuration-per-foot"}
    ,
    {UNITS_PERCENT_OBSCURATION_PER_METER, "percent-obscuration-per-meter"}
    ,
    {UNITS_MILLIOHMS, "milliohms"}
    ,
    {UNITS_MEGAWATT_HOURS, "megawatt-hours"}
    ,
    {UNITS_KILO_BTUS, "kilo-btus"}
    ,
    {UNITS_MEGA_BTUS, "mega-btus"}
    ,
    {UNITS_KILOJOULES_PER_KILOGRAM_DRY_AIR,
        "kilojoules-per-kilogram-dry-air"}
    ,
    {UNITS_MEGAJOULES_PER_KILOGRAM_DRY_AIR,
        "megajoules-per-kilogram-dry-air"}
    ,
    {UNITS_KILOJOULES_PER_DEGREE_KELVIN, "kilojoules-per-degree-Kelvin"}
    ,
    {UNITS_MEGAJOULES_PER_DEGREE_KELVIN, "megajoules-per-degree-Kelvin"}
    ,
    {UNITS_NEWTON, "newton"}
    ,
    {UNITS_GRAMS_PER_SECOND, "grams-per-second"}
    ,
    {UNITS_GRAMS_PER_MINUTE, "grams-per-minute"}
    ,
    {UNITS_TONS_PER_HOUR, "tons-per-hour"}
    ,
    {UNITS_KILO_BTUS_PER_HOUR, "kilo-btus-per-hour"}
    ,
    {UNITS_HUNDREDTHS_SECONDS, "hundredths-seconds"}
    ,
    {UNITS_MILLISECONDS, "milliseconds"}
    ,
    {UNITS_NEWTON_METERS, "newton-meters"}
    ,
    {UNITS_MILLIMETERS_PER_SECOND, "millimeters-per-second"}
    ,
    {UNITS_MILLIMETERS_PER_MINUTE, "millimeters-per-minute"}
    ,
    {UNITS_METERS_PER_MINUTE, "meters-per-minute"}
    ,
    {UNITS_METERS_PER_HOUR, "meters-per-hour"}
    ,
    {UNITS_CUBIC_METERS_PER_MINUTE, "cubic-meters-per-minute"}
    ,
    {UNITS_METERS_PER_SECOND_PER_SECOND, "meters-per-second-per-second"}
    ,
    {UNITS_AMPERES_PER_METER, "amperes-per-meter"}
    ,
    {UNITS_AMPERES_PER_SQUARE_METER, "amperes-per-square-meter"}
    ,
    {UNITS_AMPERE_SQUARE_METERS, "ampere-square-meters"}
    ,
    {UNITS_FARADS, "farads"}
    ,
    {UNITS_HENRYS, "henrys"}
    ,
    {UNITS_OHM_METERS, "ohm-meters"}
    ,
    {UNITS_SIEMENS, "siemens"}
    ,
    {UNITS_SIEMENS_PER_METER, "siemens-per-meter"}
    ,
    {UNITS_TESLAS, "teslas"}
    ,
    {UNITS_VOLTS_PER_DEGREE_KELVIN, "volts-per-degree-Kelvin"}
    ,
    {UNITS_VOLTS_PER_METER, "volts-per-meter"}
    ,
    {UNITS_WEBERS, "webers"}
    ,
    {UNITS_CANDELAS, "candelas"}
    ,
    {UNITS_CANDELAS_PER_SQUARE_METER, "candelas-per-square-meter"}
    ,
    {UNITS_DEGREES_KELVIN_PER_HOUR, "degrees-Kelvin-per-hour"}
    ,
    {UNITS_DEGREES_KELVIN_PER_MINUTE, "degrees-Kelvin-per-minute"}
    ,
    {UNITS_JOULE_SECONDS, "joule-seconds"}
    ,
    {UNITS_RADIANS_PER_SECOND, "radians-per-second"}
    ,
    {UNITS_SQUARE_METERS_PER_NEWTON, "square-meters-per-Newton"}
    ,
    {UNITS_KILOGRAMS_PER_CUBIC_METER, "kilograms-per-cubic-meter"}
    ,
    {UNITS_NEWTON_SECONDS, "newton-seconds"}
    ,
    {UNITS_NEWTONS_PER_METER, "newtons-per-meter"}
    ,
    {UNITS_WATTS_PER_METER_PER_DEGREE_KELVIN,
        "watts-per-meter-per-degree-Kelvin"}
    ,
    {0, NULL}
    /* Enumerated values 0-255 are reserved for definition by ASHRAE.
       Enumerated values 256-65535 may be used by others subject to
       the procedures and constraints described in Clause 23. */
};

INDTEXT_DATA bacnet_binary_polarity_names[] = {
    {POLARITY_NORMAL, "normal"}
    ,
    {POLARITY_REVERSE, "reverse"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_binary_present_value_names[] = {
    {BINARY_INACTIVE, "inactive"}
    ,
    {BINARY_ACTIVE, "active"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_reliability_names[] = {
    {RELIABILITY_NO_FAULT_DETECTED, "no-fault-detected"}
    ,
    {RELIABILITY_NO_SENSOR, "no-sensor"}
    ,
    {RELIABILITY_OVER_RANGE, "over-range"}
    ,
    {RELIABILITY_UNDER_RANGE, "under-range"}
    ,
    {RELIABILITY_OPEN_LOOP, "open-loop"}
    ,
    {RELIABILITY_SHORTED_LOOP, "shorted-loop"}
    ,
    {RELIABILITY_NO_OUTPUT, "no-output"}
    ,
    {RELIABILITY_UNRELIABLE_OTHER, "unreliable-other"}
    ,
    {RELIABILITY_PROCESS_ERROR, "process-error"}
    ,
    {RELIABILITY_MULTI_STATE_FAULT, "mult-state-fault"}
    ,
    {RELIABILITY_CONFIGURATION_ERROR, "configuration-error"}
    ,
    {RELIABILITY_MEMBER_FAULT, "member-fault"}
    ,
    {RELIABILITY_COMMUNICATION_FAILURE, "communication-failure"}
    ,
    {RELIABILITY_TRIPPED, "tripped"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_device_status_names[] = {
    {STATUS_OPERATIONAL, "operational"}
    ,
    {STATUS_OPERATIONAL_READ_ONLY, "operational-read-only"}
    ,
    {STATUS_DOWNLOAD_REQUIRED, "download-required"}
    ,
    {STATUS_DOWNLOAD_IN_PROGRESS, "download-in-progress"}
    ,
    {STATUS_NON_OPERATIONAL, "non-operational"}
    ,
    {STATUS_BACKUP_IN_PROGRESS, "backup-in-progress"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_segmentation_names[] = {
    {SEGMENTATION_BOTH, "segmented-both"}
    ,
    {SEGMENTATION_TRANSMIT, "segmented-transmit"}
    ,
    {SEGMENTATION_RECEIVE, "segmented-receive"}
    ,
    {SEGMENTATION_NONE, "no-segmentation"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_node_type_names[] = {
    {BACNET_NODE_UNKNOWN, "unknown"}
    ,
    {BACNET_NODE_SYSTEM, "system"}
    ,
    {BACNET_NODE_NETWORK, "network"}
    ,
    {BACNET_NODE_DEVICE, "device"}
    ,
    {BACNET_NODE_ORGANIZATIONAL, "organizational"}
    ,
    {BACNET_NODE_AREA, "area"}
    ,
    {BACNET_NODE_EQUIPMENT, "equipment"}
    ,
    {BACNET_NODE_POINT, "point"}
    ,
    {BACNET_NODE_COLLECTION, "collection"}
    ,
    {BACNET_NODE_PROPERTY, "property"}
    ,
    {BACNET_NODE_FUNCTIONAL, "functional"}
    ,
    {BACNET_NODE_OTHER, "other"}
    ,
    {0, NULL}
};

/* note: different than DaysOfWeek bit string where 0=monday */
INDTEXT_DATA bacnet_day_of_week_names[] = {
    {1, "Monday"}
    ,
    {2, "Tuesday"}
    ,
    {3, "Wednesday"}
    ,
    {4, "Thursday"}
    ,
    {5, "Friday"}
    ,
    {6, "Saturday"}
    ,
    {7, "Sunday"}
    ,
    {255, "any day of week"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_month_names[] = {
    {1, "January"}
    ,
    {2, "February"}
    ,
    {3, "March"}
    ,
    {4, "April"}
    ,
    {5, "May"}
    ,
    {6, "June"}
    ,
    {7, "July"}
    ,
    {8, "August"}
    ,
    {9, "September"}
    ,
    {10, "October"}
    ,
    {11, "November"}
    ,
    {12, "December"}
    ,
    {13, "Odd Months"}
    ,
    {14, "Even Months"}
    ,
    {255, "Any Month"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_error_class_names[] = {
    {ERROR_CLASS_DEVICE, "device"}
    ,
    {ERROR_CLASS_OBJECT, "object"}
    ,
    {ERROR_CLASS_PROPERTY, "property"}
    ,
    {ERROR_CLASS_RESOURCES, "resources"}
    ,
    {ERROR_CLASS_SECURITY, "security"}
    ,
    {ERROR_CLASS_SERVICES, "services"}
    ,
    {ERROR_CLASS_VT, "vt"}
    ,
    {0, NULL}
};

INDTEXT_DATA bacnet_error_code_names[] = {
    {ERROR_CODE_OTHER, "other"}
    ,
    {ERROR_CODE_AUTHENTICATION_FAILED, "authentication-failed"}
    ,
    {ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED, "character-set-not-supported"}
    ,
    {ERROR_CODE_CONFIGURATION_IN_PROGRESS, "configuration-in-progress"}
    ,
    {ERROR_CODE_DATATYPE_NOT_SUPPORTED, "datatype-not-supported"}
    ,
    {ERROR_CODE_DEVICE_BUSY, "device-busy"}
    ,
    {ERROR_CODE_DUPLICATE_NAME, "duplicate-name"}
    ,
    {ERROR_CODE_DUPLICATE_OBJECT_ID, "duplicate-object-id"}
    ,
    {ERROR_CODE_DYNAMIC_CREATION_NOT_SUPPORTED,
        "dynamic-creation-not-supported"}
    ,
    {ERROR_CODE_FILE_ACCESS_DENIED, "file-access-denied"}
    ,
    {ERROR_CODE_INCOMPATIBLE_SECURITY_LEVELS,
        "incompatible-security-levels"}
    ,
    {ERROR_CODE_INCONSISTENT_PARAMETERS, "inconsistent-parameters"}
    ,
    {ERROR_CODE_INCONSISTENT_SELECTION_CRITERION,
        "inconsistent-selection-criterion"}
    ,
    {ERROR_CODE_INVALID_ARRAY_INDEX, "invalid-array-index"}
    ,
    {ERROR_CODE_INVALID_CONFIGURATION_DATA, "invalid-configuration-data"}
    ,
    {ERROR_CODE_INVALID_DATA_TYPE, "invalid-data-type"}
    ,
    {ERROR_CODE_INVALID_FILE_ACCESS_METHOD, "invalid-file-access-method"}
    ,
    {ERROR_CODE_INVALID_FILE_START_POSITION,
        "error-code-invalid-file-start-position"}
    ,
    {ERROR_CODE_INVALID_OPERATOR_NAME, "invalid-operator-name"}
    ,
    {ERROR_CODE_INVALID_PARAMETER_DATA_TYPE, "invalid-parameter-data-type"}
    ,
    {ERROR_CODE_INVALID_TIME_STAMP, "invalid-time-stamp"}
    ,
    {ERROR_CODE_KEY_GENERATION_ERROR, "key-generation-error"}
    ,
    {ERROR_CODE_MISSING_REQUIRED_PARAMETER, "missing-required-parameter"}
    ,
    {ERROR_CODE_NO_OBJECTS_OF_SPECIFIED_TYPE,
        "no-objects-of-specified-type"}
    ,
    {ERROR_CODE_NO_SPACE_FOR_OBJECT, "no-space-for-object"}
    ,
    {ERROR_CODE_NO_SPACE_TO_ADD_LIST_ELEMENT,
        "no-space-to-add-list-element"}
    ,
    {ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY, "no-space-to-write-property"}
    ,
    {ERROR_CODE_NO_VT_SESSIONS_AVAILABLE, "no-vt-sessions-available"}
    ,
    {ERROR_CODE_OBJECT_DELETION_NOT_PERMITTED,
        "object-deletion-not-permitted"}
    ,
    {ERROR_CODE_OBJECT_IDENTIFIER_ALREADY_EXISTS,
        "object-identifier-already-exists"}
    ,
    {ERROR_CODE_OPERATIONAL_PROBLEM, "operational-problem"}
    ,
    {ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED,
        "optional-functionality-not-supported"}
    ,
    {ERROR_CODE_PASSWORD_FAILURE, "password-failure"}
    ,
    {ERROR_CODE_PROPERTY_IS_NOT_A_LIST, "property-is-not-a-list"}
    ,
    {ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY, "property-is-not-an-array"}
    ,
    {ERROR_CODE_READ_ACCESS_DENIED, "read-access-denied"}
    ,
    {ERROR_CODE_SECURITY_NOT_SUPPORTED, "security-not-supported"}
    ,
    {ERROR_CODE_SERVICE_REQUEST_DENIED, "service-request-denied"}
    ,
    {ERROR_CODE_TIMEOUT, "timeout"}
    ,
    {ERROR_CODE_UNKNOWN_OBJECT, "unknown-object"}
    ,
    {ERROR_CODE_UNKNOWN_PROPERTY, "unknown-property"}
    ,
    {ERROR_CODE_RESERVED1, "reserved1"}
    ,
    {ERROR_CODE_UNKNOWN_VT_CLASS, "unknown-vt-class"}
    ,
    {ERROR_CODE_UNKNOWN_VT_SESSION, "unknown-vt-session"}
    ,
    {ERROR_CODE_UNSUPPORTED_OBJECT_TYPE, "unsupported-object-type"}
    ,
    {ERROR_CODE_VALUE_OUT_OF_RANGE, "value-out-of-range"}
    ,
    {ERROR_CODE_VT_SESSION_ALREADY_CLOSED, "vt-session-already-closed"}
    ,
    {ERROR_CODE_VT_SESSION_TERMINATION_FAILURE,
        "vt-session-termination-failure"}
    ,
    {ERROR_CODE_WRITE_ACCESS_DENIED, "write-access-denied"}
    ,
    {ERROR_CODE_COV_SUBSCRIPTION_FAILED, "cov-subscription-failed"}
    ,
    {ERROR_CODE_NOT_COV_PROPERTY, "not-cov-property"}
    ,
    {ERROR_CODE_ABORT_BUFFER_OVERFLOW, "abort-buffer-overflow"}
    ,
    {ERROR_CODE_ABORT_INVALID_APDU_IN_THIS_STATE,
        "abort-invalid-apdu-in-this-state"}
    ,
    {ERROR_CODE_ABORT_PREEMPTED_BY_HIGHER_PRIORITY_TASK,
        "abort-preempted-by-higher-priority-task"}
    ,
    {ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED,
        "abort-segmentation-not-supported"}
    ,
    {ERROR_CODE_ABORT_PROPRIETARY, "abort-proprietary"}
    ,
    {ERROR_CODE_ABORT_OTHER, "abort-other"}
    ,
    {ERROR_CODE_INVALID_TAG, "invalid-tag"}
    ,
    {ERROR_CODE_NETWORK_DOWN, "network-down"}
    ,
    {ERROR_CODE_REJECT_BUFFER_OVERFLOW, "reject-buffer-overflow"}
    ,
    {ERROR_CODE_REJECT_INCONSISTENT_PARAMETERS,
        "reject-inconsistent-parameters"}
    ,
    {ERROR_CODE_REJECT_INVALID_PARAMETER_DATA_TYPE,
        "reject-invalid-parameter-data-type"}
    ,
    {ERROR_CODE_REJECT_INVALID_TAG, "reject-invalid-tag"}
    ,
    {ERROR_CODE_REJECT_MISSING_REQUIRED_PARAMETER,
        "reject-missing-required-parameter"}
    ,
    {ERROR_CODE_REJECT_PARAMETER_OUT_OF_RANGE, "reject-parameter-out-of-range"}
    ,
    {ERROR_CODE_REJECT_TOO_MANY_ARGUMENTS, "reject-too-many-arguments"}
    ,
    {ERROR_CODE_REJECT_UNDEFINED_ENUMERATION, "reject-undefined-enumeration"}
    ,
    {ERROR_CODE_REJECT_UNRECOGNIZED_SERVICE, "reject-unrecognized-service"}
    ,
    {ERROR_CODE_REJECT_PROPRIETARY, "reject-proprietary"}
    ,
    {ERROR_CODE_REJECT_OTHER, "reject-other"}
    ,
    {ERROR_CODE_UNKNOWN_DEVICE, "unknown-device"}
    ,
    {ERROR_CODE_UNKNOWN_ROUTE, "unknown-route"}
    ,
    {ERROR_CODE_VALUE_NOT_INITIALIZED, "value-not-initialized"}
    ,
    {ERROR_CODE_INVALID_EVENT_STATE, "invalid-event-state"}
    ,
    {ERROR_CODE_NO_ALARM_CONFIGURED, "no-alarm-configured"}
    ,
    {ERROR_CODE_LOG_BUFFER_FULL, "log-buffer-full"}
    ,
    {ERROR_CODE_LOGGED_VALUE_PURGED, "logged-value-purged"}
    ,
    {ERROR_CODE_NO_PROPERTY_SPECIFIED, "no-property-specified"}
    ,
    {ERROR_CODE_NOT_CONFIGURED_FOR_TRIGGERED_LOGGING,
        "not-configured-for-triggered-logging"}
    ,
    {ERROR_CODE_UNKNOWN_SUBSCRIPTION, "unknown-subscription"}
    ,
    {ERROR_CODE_PARAMETER_OUT_OF_RANGE, "parameter-out-of-range"}
    ,
    {ERROR_CODE_LIST_ELEMENT_NOT_FOUND, "list-element-not-found"}
    ,
    {ERROR_CODE_BUSY, "busy"}
    ,
    {ERROR_CODE_COMMUNICATION_DISABLED, "communication-disabled"}
    ,
    {ERROR_CODE_COMMUNICATION_DISABLED, "access-denied"}
    ,
    {ERROR_CODE_SUCCESS, "success"},
    {ERROR_CODE_ACCESS_DENIED, "access-denied"},
    {ERROR_CODE_BAD_DESTINATION_ADDRESS, "bad-destination-address"},
    {ERROR_CODE_BAD_DESTINATION_DEVICE_ID, "bad-destination-device-id"},
    {ERROR_CODE_BAD_SIGNATURE, "bad-signature"},
    {ERROR_CODE_BAD_SOURCE_ADDRESS, "bad-source-address"},
    {ERROR_CODE_BAD_TIMESTAMP, "bad-timestamp"},
    {ERROR_CODE_CANNOT_USE_KEY, "cannot-use-key"},
    {ERROR_CODE_CANNOT_VERIFY_MESSAGE_ID, "cannot-verify-message-id"},
    {ERROR_CODE_CORRECT_KEY_REVISION, "correct-key-revision"},
    {ERROR_CODE_DESTINATION_DEVICE_ID_REQUIRED, "destination-device-id-required"},
    {ERROR_CODE_DUPLICATE_MESSAGE, "duplicate-message"},
    {ERROR_CODE_ENCRYPTION_NOT_CONFIGURED, "encryption-not-configured"},
    {ERROR_CODE_ENCRYPTION_REQUIRED, "encryption-required"},
    {ERROR_CODE_INCORRECT_KEY, "incorrect-key"},
    {ERROR_CODE_INVALID_KEY_DATA, "invalid-key-data"},
    {ERROR_CODE_KEY_UPDATE_IN_PROGRESS, "key-update-in-progress"},
    {ERROR_CODE_MALFORMED_MESSAGE, "malformed-message"},
    {ERROR_CODE_NOT_KEY_SERVER, "not-key-server"},
    {ERROR_CODE_SECURITY_NOT_CONFIGURED, "security-not-configured"},
    {ERROR_CODE_SOURCE_SECURITY_REQUIRED, "source-security-required"},
    {ERROR_CODE_TOO_MANY_KEYS, "too-many-keys"},
    {ERROR_CODE_UNKNOWN_AUTHENTICATION_TYPE, "unknown-authentication-type"},
    {ERROR_CODE_UNKNOWN_KEY, "unknown-key"},
    {ERROR_CODE_UNKNOWN_KEY_REVISION, "unknown-key-revision"},
    {ERROR_CODE_UNKNOWN_SOURCE_MESSAGE, "unknown-source-message"},
    {ERROR_CODE_NOT_ROUTER_TO_DNET, "not-router-to-dnet"},
    {ERROR_CODE_ROUTER_BUSY, "router-busy"},
    {ERROR_CODE_UNKNOWN_NETWORK_MESSAGE, "unknown-network-message"},
    {ERROR_CODE_MESSAGE_TOO_LONG, "message-too-long"},
    {ERROR_CODE_SECURITY_ERROR, "security-error"},
    {ERROR_CODE_ADDRESSING_ERROR, "addressing-error"},
    {ERROR_CODE_WRITE_BDT_FAILED, "write-bdt-failed"},
    {ERROR_CODE_READ_BDT_FAILED, "read-bdt-failed"},
    {ERROR_CODE_REGISTER_FOREIGN_DEVICE_FAILED, "register-foreign-device-failed"},
    {ERROR_CODE_READ_FDT_FAILED, "read-fdt-failed"},
    {ERROR_CODE_DELETE_FDT_ENTRY_FAILED, "delete-fdt-entry-failed"},
    {ERROR_CODE_DISTRIBUTE_BROADCAST_FAILED, "distribute-broadcast-failed"},
    {ERROR_CODE_UNKNOWN_FILE_SIZE, "unknown-file-size"},
    {ERROR_CODE_ABORT_APDU_TOO_LONG, "abort-apdu-too-long"},
    {ERROR_CODE_ABORT_APPLICATION_EXCEEDED_REPLY_TIME, "abort-application-exceeded-reply-time"},
    {ERROR_CODE_ABORT_OUT_OF_RESOURCES, "abort-out-of-resources"},
    {ERROR_CODE_ABORT_TSM_TIMEOUT, "abort-tsm-timeout"},
    {ERROR_CODE_ABORT_WINDOW_SIZE_OUT_OF_RANGE, "abort-window-size-out-of-range"},
    {ERROR_CODE_FILE_FULL, "file-full"},
    {ERROR_CODE_INCONSISTENT_CONFIGURATION, "inconsistent-configuration"},
    {ERROR_CODE_INCONSISTENT_OBJECT_TYPE, "inconsistent-object-type"},
    {ERROR_CODE_INTERNAL_ERROR, "internal-error"},
    {ERROR_CODE_NOT_CONFIGURED, "not-configured"},
    {ERROR_CODE_OUT_OF_MEMORY, "out-of-memory"},
    {ERROR_CODE_VALUE_TOO_LONG, "value-too-long"},
    {ERROR_CODE_ABORT_INSUFFICIENT_SECURITY, "abort-insufficient-security"},
    {ERROR_CODE_ABORT_SECURITY_ERROR, "abort-security-error"},
    {0, NULL}
};

const char *bactext_object_type_name(uint32_t index)
{
    return indtext_by_index_split_default(bacnet_object_type_names, index, 128,
        ASHRAE_Reserved_String, Vendor_Proprietary_String);
}

const char *bactext_property_name(uint32_t index)
{
    return indtext_by_index_split_default(bacnet_property_names, index, 512,
        ASHRAE_Reserved_String, Vendor_Proprietary_String);
}

const char *bactext_event_state_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_event_state_names, index, ASHRAE_Reserved_String);
}

const char *bactext_engineering_unit_name(uint32_t index)
{
    return indtext_by_index_split_default(bacnet_engineering_unit_names, index, 256, 
            ASHRAE_Reserved_String, Vendor_Proprietary_String);
}

const char *bactext_binary_polarity_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_binary_polarity_names, index, ASHRAE_Reserved_String);
}

const char *bactext_binary_present_value_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_binary_present_value_names, index, 
            ASHRAE_Reserved_String);
}

const char *bactext_reliability_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_reliability_names, index, ASHRAE_Reserved_String);
}

const char *bactext_device_status_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_device_status_names, index, ASHRAE_Reserved_String);
}

const char *bactext_segmentation_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_segmentation_names, index, ASHRAE_Reserved_String);
}

const char *bactext_node_type_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_node_type_names, index, ASHRAE_Reserved_String);
}

const char *bactext_day_of_week_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_day_of_week_names, index, ASHRAE_Reserved_String);
}

const char *bactext_month_name(uint32_t index)
{
    return indtext_by_index_default(bacnet_month_names, index, ASHRAE_Reserved_String);
}

const char *bactext_error_class_name(uint32_t index)
{
    return indtext_by_index_split_default(bacnet_error_class_names, index, 
        FIRST_PROPRIETARY_ERROR_CLASS, ASHRAE_Reserved_String, Vendor_Proprietary_String);
}

const char *bactext_error_code_name(uint32_t index)
{
    return indtext_by_index_split_default(bacnet_error_code_names, index,
        FIRST_PROPRIETARY_ERROR_CLASS, ASHRAE_Reserved_String, Vendor_Proprietary_String);
}

int bactext_tolower(const char *src, char dst[], size_t dst_size)
{
    int len;
    
    if ((src == NULL) || (dst == NULL) || (dst_size == 0)) {
        return -EINVAL;
    }

    len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }

    dst[len] = 0;
    while (len--) {
        dst[len] = tolower(src[len]);
    }
    
    return 0;
}

int bactext_get_object_type_from_name(const char *name)
{
    char str[64];
    int rv;
    
    if (name == NULL) {
        return -EINVAL;
    }

    rv = bactext_tolower(name, str, sizeof(str));
    if (rv < 0) {
        return rv;
    }

    if ((strcmp(str, "ai") == 0) || (strcmp(str, "analog input") == 0)) {
        return OBJECT_ANALOG_INPUT;
    } else if ((strcmp(str, "ao") == 0) || (strcmp(str, "analog output") == 0)) {
        return OBJECT_ANALOG_OUTPUT;
    } else if ((strcmp(str, "av") == 0) || (strcmp(str, "analog value") == 0)) {
        return OBJECT_ANALOG_VALUE;
    } else if ((strcmp(str, "bi") == 0) || (strcmp(str, "binary input") == 0)) {
        return OBJECT_BINARY_INPUT;
    } else if ((strcmp(str, "bo") == 0) || (strcmp(str, "binary output") == 0)) {
        return OBJECT_BINARY_OUTPUT;
    } else if ((strcmp(str, "bv") == 0) || (strcmp(str, "binary value") == 0)) {
        return OBJECT_BINARY_VALUE;
    } else if ((strcmp(str, "msi") == 0) || (strcmp(str, "multistate input") == 0)) {
        return OBJECT_MULTI_STATE_INPUT;
    } else if ((strcmp(str, "mso") == 0) || (strcmp(str, "multistate output") == 0)) {
        return OBJECT_MULTI_STATE_OUTPUT;
    } else if ((strcmp(str, "msv") == 0) || (strcmp(str, "multistate value") == 0)) {
        return OBJECT_MULTI_STATE_VALUE;
    } else if ((strcmp(str, "tl") == 0) || (strcmp(str, "trendlog") == 0)) {
        return OBJECT_TRENDLOG;
    } else {
        return -EPERM;
    }

    return 0;
}

#include "vendor_def.h"

const char *bactext_vendor_name(uint16_t vendor_id)
{
    if (vendor_id >= sizeof(vendor_name) / sizeof(vendor_name[0]))
        return NULL;

    return vendor_name[vendor_id];
}

