
//#define BTSTACK_FILE__ "gatt_counter.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gatt_counter.h"
#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
// #include "gatt_browser.h"

#define HEARTBEAT_PERIOD_MS 1000



const uint LED_PIN = 4;

typedef struct advertising_report
{
    uint8_t type;
    uint8_t event_type;
    uint8_t address_type;
    bd_addr_t address;
    int rssi;
    uint8_t length;
    const uint8_t *data;
} advertising_report_t;

// START OF KALMAN Filter

static bd_addr_t cmdline_addr;
static int cmdline_addr_found = 0;

static hci_con_handle_t connection_handle;
static gatt_client_service_t services[40];
static int service_count = 0;
static int service_index = 0;

/* LISTING_START(MainConfiguration): Init L2CAP SM ATT Server and start heartbeat timer */
static int le_notification_enabled;
static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static hci_con_handle_t con_handle;
static uint8_t battery = 100;

#ifdef ENABLE_GATT_OVER_CLASSIC
static uint8_t gatt_service_buffer[70];
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static void heartbeat_handler(struct btstack_timer_source *ts);
static void beat(void);
static void dump_advertising_report(advertising_report_t *e);

// for the Scanner
static void handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// Flags general discoverable, BR/EDR supported (== not supported flag not set) when ENABLE_GATT_OVER_CLASSIC is enabled
#ifdef ENABLE_GATT_OVER_CLASSIC
#define APP_AD_FLAGS 0x02
#else
#define APP_AD_FLAGS 0x06
#endif

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02,
    BLUETOOTH_DATA_TYPE_FLAGS,
    APP_AD_FLAGS,
    // Name
    0x0b,
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'L',
    'E',
    ' ',
    'B',
    'E',
    'A',
    'C',
    'O',
    'N',
    'N',
    // Incomplete List of 16-bit Service Class UUIDs -- FF10 - only valid for testing!
    0x03,
    BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
    0x10,
    0xff,
};
const uint8_t adv_data_len = sizeof(adv_data);

/* LISTING_START(GATTBrowserQueryHandler): Handling of the GATT client queries */

static void gatt_client_setup(void)
{

    // Initialize L2CAP and register HCI event handler
    l2cap_init();

    // Initialize GATT client
    gatt_client_init();

    // Optinoally, Setup security manager
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    // register for HCI events
    hci_event_callback_registration.callback = &handle_hci_event;
    hci_add_event_handler(&hci_event_callback_registration);
}

static void le_counter_setup(void)
{

    l2cap_init();

    // setup SM: Display only
    sm_init();

#ifdef ENABLE_GATT_OVER_CLASSIC
    // init SDP, create record for GATT and register with SDP
    sdp_init();
    memset(gatt_service_buffer, 0, sizeof(gatt_service_buffer));
    gatt_create_sdp_record(gatt_service_buffer, 0x10001, ATT_SERVICE_GATT_SERVICE_START_HANDLE, ATT_SERVICE_GATT_SERVICE_END_HANDLE);
    sdp_register_service(gatt_service_buffer);
    printf("SDP service record size: %u\n", de_get_len(gatt_service_buffer));

    // configure Classic GAP
    gap_set_local_name("GATT Counter BR/EDR 00:00:00:00:00:00");
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_discoverable_control(1);
#endif

    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);

    // setup battery service
    battery_service_server_init(battery);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t *)adv_data);
    gap_advertisements_enable(1);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ATT event
    att_server_register_packet_handler(packet_handler);

    // set one-shot timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(&heartbeat);

    // beat once
    // beat();
}
/* LISTING_END */

/*
 * @section Heartbeat Handler
 *
 * @The heartbeat handler updates the value of the single Characteristic provided
 * and request a ATT_EVENT_CAN_SEND_NOW to send a notification if enabled see Listing heartbeat.
 */

/* LISTING_START(heartbeat): Hearbeat Handler */
static int counter = 0;
static char counter_string[50];
static int counter_string_len;

// float calculateDistance(int r)
// {
//     float distance = pow(10, (-60 - r) / (10 * 2.4));
//     return round(distance * 100.0) / 100.0; // Round to 4 decimal places
// }

static void beat(void)
{
    counter++;
    counter_string_len = snprintf(counter_string, sizeof(counter_string), "BTstack counter %04u", counter);
    puts(counter_string);
}

static void heartbeat_handler(struct btstack_timer_source *ts)
{

    // beat();
    if (le_notification_enabled)
    {
        // beat();
        // dump_advertising_report();
        // dump_advertising_report()
        att_server_request_can_send_now_event(con_handle);
    }

    // simulate battery drain
    // battery--;
    // if (battery < 50)
    // {
    //     battery = 100;
    // }
    battery_service_server_set_battery_value(battery);

    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}
/* LISTING_END */

/*
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - stop the counter after a disconnect
 *        - send a notification when the requested ATT_EVENT_CAN_SEND_NOW is received
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;

    switch (hci_event_packet_get_type(packet))
    {
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        le_notification_enabled = 0;
        break;
    case ATT_EVENT_CAN_SEND_NOW:
        att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t *)counter_string, counter_string_len);
        break;
    default:
        break;
    }
}

/* LISTING_END */

/*
 * @section ATT Read
 *
 * @text The ATT Server handles all reads to constant data. For dynamic data like the custom characteristic, the registered
 * att_read_callback is called. To handle long characteristics and long reads, the att_read_callback is first called
 * with buffer == NULL, to request the total value length. Then it will be called again requesting a chunk of the value.
 * See Listing attRead.
 */

/* LISTING_START(attRead): ATT Read */

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    UNUSED(connection_handle);

    if (att_handle == ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE)
    {
        return att_read_callback_handle_blob((const uint8_t *)counter_string, counter_string_len, offset, buffer, buffer_size);
    }
    return 0;
}
/* LISTING_END */

/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored and used
 * in the heartbeat handler to decide if a new value should be sent. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer_size);

    if (att_handle != ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE)
        return 0;
    le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
    con_handle = connection_handle;
    return 0;
}
/* LISTING_END */

static void printUUID(uint8_t *uuid128, uint16_t uuid16)
{
    if (uuid16)
    {
        printf("%04x", uuid16);
    }
    else
    {
        printf("%s", uuid128_to_str(uuid128));
    }
}

// static void dump_advertising_report(advertising_report_t *e)
// {

//     bd_addr_t cmdline_addr = {0xE8, 0xDB, 0x84, 0x12, 0x9D, 0x26};
//     bd_addr_t mac_addr0 = {0xE8, 0xDB, 0x84, 0x12, 0x9D, 0x26};
//     bd_addr_t mac_addr1 = {0x84, 0xFC, 0xE6, 0x84, 0x3D, 0xE2};
//     bd_addr_t mac_addr2 = {0x84, 0xFC, 0xE6, 0x84, 0x4A, 0x1E};
//     bd_addr_t mac_addr3 = {0x84, 0xFC, 0xE6, 0x84, 0x23, 0xE2};
//     bd_addr_t mac_addr4 = {0x84, 0xFC, 0xE6, 0x85, 0x44, 0xC2};
//     bd_addr_t mac_addr5 = {0x84, 0xFC, 0xE6, 0x85, 0x43, 0xB6};
//     //{

//     // counter_string_len = snprintf(counter_string, sizeof(counter_string), "FARWAY-TAG:, %u, %s, %f, %d", e->rssi, bd_addr_to_str(e->address));
//     // puts(counter_string);

//     if (memcmp(e->address, mac_addr0, 6) == 0)
//     {
//         // Address matches cmdline_addr
//         float distance = calculateDistance(e->rssi);

//         // int converted_rssi = e->rssi;

//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TEST-TAG:, %u, %s, %f, %d", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);

//         if (e->rssi > 170)
//         {
//             counter_string_len = snprintf(counter_string, sizeof(counter_string), "NEAR-TAG:, %u, %s, %f, %d", e->rssi, bd_addr_to_str(e->address));
//             puts(counter_string);

//             cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
//         }
//         else if (e->rssi < 170)
//         {

//             counter_string_len = snprintf(counter_string, sizeof(counter_string), "FAR-TAG:, %u, %s, %f, %d", e->rssi, bd_addr_to_str(e->address));
//             puts(counter_string);
//         }
//     }
//     else if (memcmp(e->address, mac_addr1, 6) == 0)
//     {
//         //
//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TAG-1:, %u, %s", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);
//     }
//     else if (memcmp(e->address, mac_addr2, 6) == 0)
//     {

//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TAG-2:, %u, %s", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);
//     }
//     else if (memcmp(e->address, mac_addr3, 6) == 0)
//     {
//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TAG-3:, %u, %s", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);
//     }
//     else if (memcmp(e->address, mac_addr4, 6) == 0)
//     {
//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TAG-4:, %u, %s", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);
//     }
//     else if (memcmp(e->address, mac_addr5, 6) == 0)
//     {
//         counter_string_len = snprintf(counter_string, sizeof(counter_string), "TAG-5:, %u, %s", e->rssi, bd_addr_to_str(e->address));
//         puts(counter_string);
//     }
//     //}

//     // counter_string_len = snprintf(counter_string, sizeof(counter_string), "BTstack rssi %d", e->rssi);

//     // puts(counter_string);

//     // printf(counter_string);
// }

static void dump_characteristic(gatt_client_characteristic_t *characteristic)
{
    printf("    * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid ",
           characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, characteristic->properties);
    printUUID(characteristic->uuid128, characteristic->uuid16);
    printf("\n");
}

static void dump_service(gatt_client_service_t *service)
{
    printf("    * service: [0x%04x-0x%04x], uuid ", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
    printf("\n");
}

static void fill_advertising_report_from_packet(advertising_report_t *report, uint8_t *packet)
{
    gap_event_advertising_report_get_address(packet, report->address);
    report->event_type = gap_event_advertising_report_get_advertising_event_type(packet);
    report->address_type = gap_event_advertising_report_get_address_type(packet);
    report->rssi = gap_event_advertising_report_get_rssi(packet);
    report->length = gap_event_advertising_report_get_data_length(packet);
    report->data = gap_event_advertising_report_get_data(packet);
}

// for the scanner
static void handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET)
        return;
    advertising_report_t report;

    uint8_t event = hci_event_packet_get_type(packet);
    switch (event)
    {
    case BTSTACK_EVENT_STATE:
        // BTstack activated, get started
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING)
            break;
        if (cmdline_addr_found)
        {
            printf("Trying to connect to %s\n", bd_addr_to_str(cmdline_addr));
            gap_connect(cmdline_addr, 0);
            break;
        }
        printf("BTstack activated, start scanning!\n");
        gap_set_scan_parameters(0, 0x0030, 0x0030);
        gap_start_scan();
        break;
    case GAP_EVENT_ADVERTISING_REPORT:
        fill_advertising_report_from_packet(&report, packet);
        dump_advertising_report(&report);
        gap_start_scan();

        // stop scanning, and connect to the device
        // gap_stop_scan();
        // gap_connect(report.address, report.address_type);
        break;
    case HCI_EVENT_LE_META:
        // wait for connection complete
        if (hci_event_le_meta_get_subevent_code(packet) != HCI_SUBEVENT_LE_CONNECTION_COMPLETE)
            break;
        printf("\nGATT browser - CONNECTED\n");
        connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
        // query primary services
        gatt_client_discover_primary_services(handle_gatt_client_event, connection_handle);
        break;
    case HCI_EVENT_DISCONNECTION_COMPLETE:
        printf("\nGATT browser - DISCONNECTED\n");
        break;
    default:
        break;
    }
}

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    switch (hci_event_packet_get_type(packet))
    {
    case GATT_EVENT_SERVICE_QUERY_RESULT:
        gatt_event_service_query_result_get_service(packet, &service);
        dump_service(&service);
        services[service_count++] = service;
        break;
    case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
        dump_characteristic(&characteristic);
        break;
    case GATT_EVENT_QUERY_COMPLETE:
        // GATT_EVENT_QUERY_COMPLETE of search characteristics
        if (service_index < service_count)
        {
            service = services[service_index++];
            printf("\nGATT browser - CHARACTERISTIC for SERVICE %s, [0x%04x-0x%04x]\n",
                   uuid128_to_str(service.uuid128), service.start_group_handle, service.end_group_handle);
            gatt_client_discover_characteristics_for_service(handle_gatt_client_event, connection_handle, &service);
            break;
        }
        service_index = 0;
        break;
    default:
        break;
    }
}

int btstack_main(void);

int btstack_main(void)
{
    le_counter_setup();
    gatt_client_setup();
    // stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
