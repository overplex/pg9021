#define BTSTACK_FILE__ "pg9021.c"

#include <inttypes.h>
#include <stdio.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack.h"
#include "btstack_config.h"
#include "btstack_hid_parser.h"
#include "l2cap.h"
#include "pg9021_mapping.h"
#include "sdp_util.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

// Keys
static uint8_t keyboard_count_zeros = 0;
static uint16_t keys_states[255];
static uint16_t keyboard_last_key = 0;
static uint16_t dpad_last_key = GP_DPAD_RELEASED;

// SDP
static uint8_t hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t hid_descriptor_len;

static uint16_t hid_control_psm;
static uint16_t hid_interrupt_psm;

static uint8_t attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size =
    MAX_ATTRIBUTE_VALUE_SIZE;

// L2CAP
static uint16_t l2cap_hid_control_cid;
static uint16_t l2cap_hid_interrupt_cid;

// Remote device address
static bd_addr_t remote_addr;

// Callbacks
static gamepad_handler_t gamepad_action_callback;
static btstack_packet_callback_registration_t hci_event_callback_registration;

void set_gamepad_action_callback(gamepad_handler_t callback);
static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type,
                                           uint16_t channel, uint8_t *packet,
                                           uint16_t size);

static void clear_keys_states(void) {
  for (int i = 0; i < 255; ++i) {
    keys_states[i] = 0;
  }

  keys_states[GP_THUMB_L_X] = GP_THUMB_RELEASED;
  keys_states[GP_THUMB_L_Y] = GP_THUMB_RELEASED;
  keys_states[GP_THUMB_R_X] = GP_THUMB_RELEASED;
  keys_states[GP_THUMB_R_Y] = GP_THUMB_RELEASED;
}

void set_gamepad_mac(const char *mac) {
  // Parse human readable Bluetooth address
  sscanf_bd_addr(mac, remote_addr);
}

void connect_gamepad(void) {
  clear_keys_states();
  sdp_client_query_uuid16(
      &handle_sdp_client_query_result, remote_addr,
      BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
  printf("Trying to connect gamepad...\n");
}

void set_gamepad_action_callback(gamepad_handler_t callback) {
  gamepad_action_callback = callback;
}

static void on_device_input(uint16_t page, uint16_t usage, int32_t value) {
  if (keys_states[usage] != value) {
    keys_states[usage] = value;
    (*gamepad_action_callback)(page, usage, value);
  }
}

static uint16_t smooth_curve(uint16_t prev, uint16_t current) {
  if (current == 255 || current == 127 || current == 0) {
    return current;
  }

  if (prev == 127 &&
      ((current > 117 && current < 127) || (current > 127 && current < 137))) {
    return 127;
  }

  return (uint8_t)((prev + current) / 2);
}

static void hid_host_setup(void) {
  // Initialize L2CAP
  l2cap_init();

  // register L2CAP Services for reconnections
  l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 0xffff,
                         gap_get_security_level());
  l2cap_register_service(packet_handler, PSM_HID_CONTROL, 0xffff,
                         gap_get_security_level());

  // Allow sniff mode requests by HID device and support role switch
  gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE |
                                       LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

  // try to become master on incoming connections
  hci_set_master_slave_policy(HCI_ROLE_MASTER);

  // register for HCI events
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  // Disable stdout buffering
  setbuf(stdout, NULL);
}

/* @section SDP parser callback
 *
 * @text The SDP parsers retrieves the BNEP PAN UUID as explained in
 * Section [on SDP BNEP Query example](#sec:sdpbnepqueryExample}.
 */
static void handle_sdp_client_query_result(uint8_t packet_type,
                                           uint16_t channel, uint8_t *packet,
                                           uint16_t size) {
  UNUSED(packet_type);
  UNUSED(channel);
  UNUSED(size);

  des_iterator_t attribute_list_it;
  des_iterator_t additional_des_it;
  des_iterator_t prot_it;
  uint8_t *des_element;
  uint8_t *element;
  uint32_t uuid;
  uint8_t status;

  switch (hci_event_packet_get_type(packet)) {
    case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
      if (sdp_event_query_attribute_byte_get_attribute_length(packet) <=
          attribute_value_buffer_size) {
        attribute_value[sdp_event_query_attribute_byte_get_data_offset(
            packet)] = sdp_event_query_attribute_byte_get_data(packet);
        if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet) +
                       1) ==
            sdp_event_query_attribute_byte_get_attribute_length(packet)) {
          switch (sdp_event_query_attribute_byte_get_attribute_id(packet)) {
            case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
              for (des_iterator_init(&attribute_list_it, attribute_value);
                   des_iterator_has_more(&attribute_list_it);
                   des_iterator_next(&attribute_list_it)) {
                if (des_iterator_get_type(&attribute_list_it) != DE_DES)
                  continue;
                des_element = des_iterator_get_element(&attribute_list_it);
                des_iterator_init(&prot_it, des_element);
                element = des_iterator_get_element(&prot_it);
                if (!element) continue;
                if (de_get_element_type(element) != DE_UUID) continue;
                uuid = de_get_uuid32(element);
                des_iterator_next(&prot_it);
                switch (uuid) {
                  case BLUETOOTH_PROTOCOL_L2CAP:
                    if (!des_iterator_has_more(&prot_it)) continue;
                    de_element_get_uint16(des_iterator_get_element(&prot_it),
                                          &hid_control_psm);
                    printf("HID Control PSM: 0x%04x\n", (int)hid_control_psm);
                    break;
                  default:
                    break;
                }
              }
              break;
            case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
              for (des_iterator_init(&attribute_list_it, attribute_value);
                   des_iterator_has_more(&attribute_list_it);
                   des_iterator_next(&attribute_list_it)) {
                if (des_iterator_get_type(&attribute_list_it) != DE_DES)
                  continue;
                des_element = des_iterator_get_element(&attribute_list_it);
                for (des_iterator_init(&additional_des_it, des_element);
                     des_iterator_has_more(&additional_des_it);
                     des_iterator_next(&additional_des_it)) {
                  if (des_iterator_get_type(&additional_des_it) != DE_DES)
                    continue;
                  des_element = des_iterator_get_element(&additional_des_it);
                  des_iterator_init(&prot_it, des_element);
                  element = des_iterator_get_element(&prot_it);
                  if (!element) continue;
                  if (de_get_element_type(element) != DE_UUID) continue;
                  uuid = de_get_uuid32(element);
                  des_iterator_next(&prot_it);
                  switch (uuid) {
                    case BLUETOOTH_PROTOCOL_L2CAP:
                      if (!des_iterator_has_more(&prot_it)) continue;
                      de_element_get_uint16(des_iterator_get_element(&prot_it),
                                            &hid_interrupt_psm);
                      printf("HID Interrupt PSM: 0x%04x\n",
                             (int)hid_interrupt_psm);
                      break;
                    default:
                      break;
                  }
                }
              }
              break;
            case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
              for (des_iterator_init(&attribute_list_it, attribute_value);
                   des_iterator_has_more(&attribute_list_it);
                   des_iterator_next(&attribute_list_it)) {
                if (des_iterator_get_type(&attribute_list_it) != DE_DES)
                  continue;
                des_element = des_iterator_get_element(&attribute_list_it);
                for (des_iterator_init(&additional_des_it, des_element);
                     des_iterator_has_more(&additional_des_it);
                     des_iterator_next(&additional_des_it)) {
                  if (des_iterator_get_type(&additional_des_it) != DE_STRING)
                    continue;
                  element = des_iterator_get_element(&additional_des_it);
                  const uint8_t *descriptor = de_get_string(element);
                  hid_descriptor_len = de_get_data_size(element);
                  memcpy(hid_descriptor, descriptor, hid_descriptor_len);
                  printf("HID Descriptor:\n");
                  printf_hexdump(hid_descriptor, hid_descriptor_len);
                }
              }
              break;
            default:
              break;
          }
        }
      } else {
        fprintf(stderr,
                "SDP attribute value buffer size exceeded: available %d, "
                "required %d\n",
                attribute_value_buffer_size,
                sdp_event_query_attribute_byte_get_attribute_length(packet));
      }
      break;

    case SDP_EVENT_QUERY_COMPLETE:
      if (sdp_event_query_complete_get_status(packet) != ERROR_CODE_SUCCESS) {
        printf("SDP Query failed\n");
        break;
      }
      if ((l2cap_hid_interrupt_cid != 0) && (l2cap_hid_control_cid != 0)) {
        printf("HID device re-connected\n");
        break;
      }
      if (!hid_control_psm) {
        hid_control_psm = BLUETOOTH_PSM_HID_CONTROL;
        printf("HID Control PSM missing, using default 0x%04x\n",
               hid_control_psm);
      }
      if (!hid_interrupt_psm) {
        hid_interrupt_psm = BLUETOOTH_PSM_HID_INTERRUPT;
        printf("HID Interrupt PSM missing, using default 0x%04x\n",
               hid_interrupt_psm);
        break;
      }
      printf("Setup HID\n");
      status =
          l2cap_create_channel(packet_handler, remote_addr, hid_control_psm, 48,
                               &l2cap_hid_control_cid);
      if (status) {
        printf("Connecting to HID Control failed: 0x%02x\n", status);
      }
      break;
  }
}

static void hid_host_handle_interrupt_report(const uint8_t *report,
                                             uint16_t report_len) {
  // check if HID Input Report
  if (report_len < 1) return;
  if (*report != 0xa1) return;
  report++;
  report_len--;
  btstack_hid_parser_t parser;
  btstack_hid_parser_init(&parser, hid_descriptor, hid_descriptor_len,
                          HID_REPORT_TYPE_INPUT, report, report_len);

  while (btstack_hid_parser_has_more(&parser)) {
    uint16_t usage_page;
    uint16_t usage;
    int32_t value;
    btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);

    switch (usage_page) {
      case PAGE_KEYBOARD_BUTTONS:
        if (usage < 0xE0 || usage > 0xE7) {  // Trash
          if (usage == 0) {
            keyboard_count_zeros++;
            if (keyboard_count_zeros > 5) {  // 5 zeros between cmds
              keyboard_count_zeros = 0;
              if (keyboard_last_key != 0) {
                (*gamepad_action_callback)(usage_page, keyboard_last_key, 0);
                keyboard_last_key = 0;
              }
            }
          } else {
            keyboard_count_zeros = 0;
            if (usage != keyboard_last_key) {
              keyboard_last_key = usage;
              (*gamepad_action_callback)(usage_page, usage, 1);
            }
          }
        }
        break;

      case PAGE_GAMEPAD_DPAD_THUMB:
        if (usage == GP_USAGE_DPAD) {
          if (value == GP_DPAD_RELEASED) {  // D-pad button released
            if (dpad_last_key != GP_DPAD_RELEASED) {
              (*gamepad_action_callback)(usage_page, dpad_last_key, 0);
              dpad_last_key = GP_DPAD_RELEASED;
            }
          } else if (dpad_last_key != value) {  // D-pad button pressed
            dpad_last_key = value;
            (*gamepad_action_callback)(usage_page, value, 1);
          }
        } else {  // Thumb
          value = smooth_curve(keys_states[usage], value);
          on_device_input(usage_page, usage, value);
        }
        break;

      case PAGE_GAMEPAD_BUTTONS:
      case PAGE_MISC_ADDITIONAL_BUTTONS:
        on_device_input(usage_page, usage, value);
        break;

      default:
        printf("UNNOWN page: 0x%04x, usage: 0x%04x, value=%d\n", usage_page,
               usage, value);
        break;
    }
  }
}

/*
 * @section Packet Handler
 *
 * @text The packet handler responds to various HCI Events.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size) {
  uint8_t event;
  uint8_t status;
  bd_addr_t event_addr;
  uint16_t l2cap_cid;

  switch (packet_type) {
    case HCI_EVENT_PACKET:
      event = hci_event_packet_get_type(packet);
      switch (event) {
        /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
         * is received and the example is started in client mode, the remote SDP
         * HID query is started.
         */
        case BTSTACK_EVENT_STATE:
          if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
            printf("Start SDP HID query for remote HID Device %s.\n",
                   bd_addr_to_str(remote_addr));
            sdp_client_query_uuid16(
                &handle_sdp_client_query_result, remote_addr,
                BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
          }
          break;

        /* LISTING_PAUSE */
        case HCI_EVENT_PIN_CODE_REQUEST:
          // inform about pin code request
          printf("Pin code request - using '0000'\n");
          hci_event_pin_code_request_get_bd_addr(packet, event_addr);
          gap_pin_code_response(event_addr, "0000");
          break;

        /* LISTING_RESUME */
        case L2CAP_EVENT_INCOMING_CONNECTION:
          l2cap_cid = l2cap_event_incoming_connection_get_local_cid(packet);
          switch (l2cap_event_incoming_connection_get_psm(packet)) {
            case PSM_HID_CONTROL:
            case PSM_HID_INTERRUPT:
              l2cap_accept_connection(l2cap_cid);
              break;
            default:
              l2cap_decline_connection(l2cap_cid);
              break;
          }
          break;
        case L2CAP_EVENT_CHANNEL_OPENED:
          status = packet[2];
          if (status) {
            printf("L2CAP Connection failed: 0x%02x\n", status);
            break;
          }
          l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet);
          switch (l2cap_event_channel_opened_get_psm(packet)) {
            case PSM_HID_CONTROL:
              if (l2cap_event_channel_opened_get_incoming(packet) == 0) {
                status = l2cap_create_channel(packet_handler, remote_addr,
                                              hid_interrupt_psm, 48,
                                              &l2cap_hid_interrupt_cid);
                if (status) {
                  printf("Connecting to HID Interrupt failed: 0x%02x\n",
                         status);
                  break;
                }
              }
              l2cap_hid_control_cid = l2cap_cid;
              break;
            case PSM_HID_INTERRUPT:
              l2cap_hid_interrupt_cid = l2cap_cid;
              break;
            default:
              break;
          }

          if ((l2cap_hid_control_cid != 0) && (l2cap_hid_interrupt_cid != 0)) {
            if (hid_descriptor_len == 0) {
              printf("Start SDP HID query to get HID Descriptor\n");
              sdp_client_query_uuid16(
                  &handle_sdp_client_query_result, remote_addr,
                  BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
            } else {
              printf("HID Connection established\n");
            }
          }
          break;
        case L2CAP_EVENT_CHANNEL_CLOSED:
          if ((l2cap_hid_control_cid != 0) && (l2cap_hid_interrupt_cid != 0)) {
            printf("HID Connection closed\n");
            hid_descriptor_len = 0;
          }
          l2cap_cid = l2cap_event_channel_closed_get_local_cid(packet);
          if (l2cap_cid == l2cap_hid_control_cid) {
            l2cap_hid_control_cid = 0;
          }
          if (l2cap_cid == l2cap_hid_interrupt_cid) {
            l2cap_hid_interrupt_cid = 0;
          }
        default:
          break;
      }
      break;
    case L2CAP_DATA_PACKET:
      // for now, just dump incoming data
      if (channel == l2cap_hid_interrupt_cid) {
        hid_host_handle_interrupt_report(packet, size);
      } else if (channel == l2cap_hid_control_cid) {
        printf("HID Control: ");
        printf_hexdump(packet, size);
      } else {
        break;
      }
    default:
      break;
  }
}

int btstack_main(int argc, const char *argv[]);
int btstack_main(int argc, const char *argv[]) {
  (void)argc;
  (void)argv;

  clear_keys_states();
  hid_host_setup();
  hci_power_control(HCI_POWER_ON);

  return 0;
}
