
#include "uart_async_adapter.h"

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

#include "myBle.h"
#include "uttec.h"
#include "sx1509.h"

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

// #define DEVICE_NAME CONFIG_BT_DEVICE_NAME
// #define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

// #define RUN_STATUS_LED DK_LED1
// #define RUN_LED_BLINK_INTERVAL 3000

#define CON_STATUS_LED DK_LED2

#define KEY_PASSKEY_ACCEPT DK_BTN1_MSK
#define KEY_PASSKEY_REJECT DK_BTN2_MSK

// #define UART_BUF_SIZE CONFIG_BT_NUS_UART_BUFFER_SIZE
#define UART_BUF_SIZE 100
// #define UART_BUF_SIZE 200
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX CONFIG_BT_NUS_UART_RX_WAIT_TIME

static K_SEM_DEFINE(ble_init_ok, 0, 1);

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(nordic_nus_uart));
static struct k_work_delayable uart_work;


struct uart_data_t {
	void *fifo_reserved;
	uint8_t data[UART_BUF_SIZE];
	uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

// static const struct bt_data ad[] = {
// 	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
// 	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
// };

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

#if CONFIG_BT_NUS_UART_ASYNC_ADAPTER
UART_ASYNC_ADAPTER_INST_DEFINE(async_adapter);
#else
static const struct device *const async_adapter;
#endif



static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);

	static size_t aborted_len;
	struct uart_data_t *buf;
	static uint8_t *aborted_buf;
	static bool disable_req;

	switch (evt->type) {
	case UART_TX_DONE:
		LOG_DBG("UART_TX_DONE");
		LOG_INF("UART_TX_DONE");
		if ((evt->data.tx.len == 0) ||
		    (!evt->data.tx.buf)) {
			return;
		}

		if (aborted_buf) {
			buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
					   data);
			aborted_buf = NULL;
			aborted_len = 0;
		} else {
			buf = CONTAINER_OF(evt->data.tx.buf, struct uart_data_t,
					   data);
		}

		k_free(buf);

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		if (!buf) {
			return;
		}

		if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS)) {
			LOG_WRN("Failed to send data over UART");
		}

		break;

	case UART_RX_RDY:
	// test0
		// LOG_DBG("UART_RX_RDY");
		// LOG_INF("UART_RX_RDY");
		buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data);
		buf->len += evt->data.rx.len;

		if (disable_req) {
			return;
		}

		if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
		    (evt->data.rx.buf[buf->len - 1] == '\r')) {
			// printk("<--%d\r\n", buf->len);	
			disable_req = true;
			uart_rx_disable(uart);
		}

		break;

	case UART_RX_DISABLED:
		LOG_DBG("UART_RX_DISABLED");
		disable_req = false;

		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
			k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
			return;
		}

		uart_rx_enable(uart, buf->data, sizeof(buf->data),
			       UART_WAIT_FOR_RX);

		break;

	case UART_RX_BUF_REQUEST:
		LOG_DBG("UART_RX_BUF_REQUEST");
		buf = k_malloc(sizeof(*buf));
		if (buf) {
			buf->len = 0;
			uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
		} else {
			LOG_WRN("Not able to allocate UART receive buffer");
		}

		break;

	case UART_RX_BUF_RELEASED:
		LOG_DBG("UART_RX_BUF_RELEASED");
		buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
				   data);

		if (buf->len > 0) {
			k_fifo_put(&fifo_uart_rx_data, buf);
		} else {
			k_free(buf);
		}

		break;

	case UART_TX_ABORTED:
		LOG_DBG("UART_TX_ABORTED");
		if (!aborted_buf) {
			aborted_buf = (uint8_t *)evt->data.tx.buf;
		}

		aborted_len += evt->data.tx.len;
		buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
				   data);

		uart_tx(uart, &buf->data[aborted_len],
			buf->len - aborted_len, SYS_FOREVER_MS);

		break;

	default:
		break;
	}
}

static void uart_work_handler(struct k_work *item)
{
	struct uart_data_t *buf;

	buf = k_malloc(sizeof(*buf));
	if (buf) {
		buf->len = 0;
	} else {
		LOG_WRN("Not able to allocate UART receive buffer");
		k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
		return;
	}

	uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_WAIT_FOR_RX);
}

static bool uart_test_async_api(const struct device *dev)
{
	const struct uart_driver_api *api =
			(const struct uart_driver_api *)dev->api;

	return (api->callback_set != NULL);
}

static int uart_init(void)
{
	int err;
	// int pos;
	struct uart_data_t *rx;
	struct uart_data_t *tx;

	if (!device_is_ready(uart)) {
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_USB_DEVICE_STACK)) {
		err = usb_enable(NULL);
		if (err && (err != -EALREADY)) {
			LOG_ERR("Failed to enable USB");
			return err;
		}
	}

	rx = k_malloc(sizeof(*rx));
	if (rx) {
		rx->len = 0;
	} else {
		return -ENOMEM;
	}

	k_work_init_delayable(&uart_work, uart_work_handler);


	if (IS_ENABLED(CONFIG_BT_NUS_UART_ASYNC_ADAPTER) && !uart_test_async_api(uart)) {
		/* Implement API adapter */
		uart_async_adapter_init(async_adapter, uart);
		uart = async_adapter;
	}

	err = uart_callback_set(uart, uart_cb, NULL);
	if (err) {
		k_free(rx);
		LOG_ERR("Cannot initialize UART callback");
		return err;
	}

	if (IS_ENABLED(CONFIG_UART_LINE_CTRL)) {
		LOG_INF("Wait for DTR");
		while (true) {
			uint32_t dtr = 0;

			uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
			if (dtr) {
				break;
			}
			/* Give CPU resources to low priority threads. */
			k_sleep(K_MSEC(100));
		}
		LOG_INF("DTR set");
		err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DCD, 1);
		if (err) {
			LOG_WRN("Failed to set DCD, ret code %d", err);
		}
		err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DSR, 1);
		if (err) {
			LOG_WRN("Failed to set DSR, ret code %d", err);
		}
	}

	err = uart_rx_enable(uart, rx->data, sizeof(rx->data), 50);
	if (err) {
		LOG_ERR("Cannot enable uart reception (err: %d)", err);
		/* Free the rx buffer only because the tx buffer will be handled in the callback */
		k_free(rx);
	}

	return err;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}
	connectFlag_t* pFlags = getConnectFlag();
	pFlags->ble = true;
	pFlags->first = false;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);

	current_conn = bt_conn_ref(conn);

	dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	
	connectFlag_t* pFlags = getConnectFlag();
	pFlags->ble = false;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		dk_set_led_off(CON_STATUS_LED);
	}
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_WRN("Security failed: %s level %u err %d", addr,
			level, err);
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_NUS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	auth_conn = bt_conn_ref(conn);

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);
	LOG_INF("Press Button 1 to confirm, Button 2 to reject.");
}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing failed conn: %s, reason %d", addr, reason);
}


static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif


static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

	LOG_INF("Received from: %s, data: %s", addr, data);

	for (uint16_t pos = 0; pos != len;) {
		struct uart_data_t *tx = k_malloc(sizeof(*tx));

		if (!tx) {
			LOG_WRN("Not able to allocate UART send data buffer");
			return;
		}

		size_t tx_data_size = sizeof(tx->data) - 1;

		if ((len - pos) > tx_data_size) {
			tx->len = tx_data_size;
		} else {
			tx->len = (len - pos);
		}

		memcpy(tx->data, &data[pos], tx->len);

		pos += tx->len;

		if ((pos == len) && (data[len - 1] == '\r')) {
			tx->data[tx->len] = '\n';
			tx->len++;
		}
		procRxBle((uint8_t *)tx->data, tx->len);
		k_free(tx);	
	}
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};


#define DEVICE_NAME "wbiz_nrf_123"
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

// static const struct bt_data ad[] = {
// 	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
// 	// BT_DATA(BT_DATA_NAME_COMPLETE, adTemp, 20),
// 	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
// };

void initBle(void){
	int err = 0;
	err = uart_init();
	printk("----------------\r\n");
	printk("now start initBle. 2023.12.08. 15:13\r\n");

	if (IS_ENABLED(CONFIG_BT_NUS_SECURITY_ENABLED)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			printk("Failed to register authorization callbacks.\n");
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			printk("Failed to register authorization info callbacks.\n");
		}
	}

	err = bt_enable(NULL);

	LOG_INF("Bluetooth initialized");

	k_sem_give(&ble_init_ok);

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_nus_init(&nus_cb);
	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
	}

	whybiz_t* pFactor = getWhybizFactor();

	uint8_t test[20] = {0, }; 
	printk("ble: %d\r\n", pFactor->ble);
	sprintf(test, "wbiz_nrf_%d", pFactor->ble);
struct bt_data ad_hong[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	// BT_DATA(BT_DATA_NAME_COMPLETE, adTemp, 20),
	BT_DATA(BT_DATA_NAME_COMPLETE, test, DEVICE_NAME_LEN),
};

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad_hong, ARRAY_SIZE(ad_hong), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	}

}

void procRxBle(uint8_t* data, uint16_t len){
	printk("procRxData len: %d \r\n", len);
	uint8_t category = data[0];
	uint8_t sensor = data[1];
	uint8_t value = data[2];
	whybiz_t* pFactor = getWhybizFactor();
	printk("ca: %d, se: %d, value: %d\r\n", category, sensor, value);
	switch(category){
		case CTR_RELAY:
			printk("relay setting\r\n");
			writeOutSx(sensor, value);
		break;
		case CTR_LORA:
			printk("lora setting by ble. tbd(2023.12.14)\r\n");
			// writeOutSx(sensor, value);
		break;
		case CTR_CHANNEL:
			pFactor->channel = sensor - 1;
			setUartChannel(pFactor->channel);
			printk("channel setting by ble. tbd(2023.12.14)\r\n");
		break;
	}
}

void sendToMobile(uint8_t* buf, uint8_t len){
	connectFlag_t* pFlag = getConnectFlag();
	if(pFlag->ble){
		if (bt_nus_send(NULL, buf, len)) {
			LOG_WRN("Failed to send data over BLE connection");
		}
	}
}

void procChannel(void){
	uint8_t buf[3] = {0, };
    whybiz_t* pFactor = getWhybizFactor();

	buf[0] = CHANNEL_DEVICE; buf[1] = pFactor->channel; buf[2] = pFactor->lora_ch;
	sendToMobile(buf, sizeof(buf));
}

#include "adc.h"
void procAdcTxBle(void){
	printk("procAdcTxBle\r\n");
	readAdcValue();
	uint8_t buf[3] = {0, };
    whybiz_t* pFactor = getWhybizFactor();

	buf[0] = ADC_DEVICE; buf[1] = pFactor->adc1; buf[2] = pFactor->adc2;
	sendToMobile(buf, sizeof(buf));
}

void procSwitchTxBle(bool ble){
	readSxSw();
	if(ble){
		uint8_t buf[3] = {0, };
		whybiz_t* pFactor = getWhybizFactor();

		buf[0] = SWITCH_DEVICE; buf[1] = pFactor->sw; buf[2] = pFactor->sw;
		sendToMobile(buf, sizeof(buf));
	}
}

// void procRelayTxBle(uint8_t num, uint8_t value){
void procRelayTxBle(bool ble){
	readSxRelay();
	if(ble){
		uint8_t buf[3] = {0, };
		whybiz_t* pFactor = getWhybizFactor();

		buf[0] = RELAY_DEVICE; buf[1] = pFactor->relay; buf[2] = pFactor->relay;
		sendToMobile(buf, sizeof(buf));
	}
}

void procLora(void){
	uint8_t buf[3] = {0, };
    whybiz_t* pFactor = getWhybizFactor();

	buf[0] = LORA_DEVICE; buf[1] = pFactor->power; buf[2] = pFactor->rssi;
	sendToMobile(buf, sizeof(buf));
}

void procVersion(void){
	uint8_t buf[3] = {0, };
    whybiz_t* pFactor = getWhybizFactor();

	buf[0] = VERSION_DEVICE; buf[1] = pFactor->version; buf[2] = pFactor->ble;
	sendToMobile(buf, sizeof(buf));
}


jsonFrame_t myJson = {0, };
#define END_POSITION 
void getJsonData(uint8_t* pBuf, uint8_t len){
	bool startFlag = false;
	for(int i = 0; i < len; i++){
		myJson.frame[i] = *pBuf++;
		if((myJson.frame[i] == '{')&&(!startFlag)){
			myJson.start = i;	
			startFlag = true;
		} 
		else{//alread startFlag
			if(((i - myJson.start) == 6)&&(myJson.frame[i] == '}')){
				printf("end frame\r\n");
				myJson.end = i;
				myJson.flag = 1;
			}

		}
	} 
}

void clearJsonData(void){
	for(int i = 0; i < sizeof(myJson.frame); i++) myJson.frame[i] = 0;
	myJson.flag = myJson.end = myJson.start = 0;
}

jsonFrame_t* getJsonFrame(void){
	return &myJson;
}

void dispJsonFrame(void){

	printk("start: %d, end: %d\r\n", myJson.start, myJson.end);
	for(int i = 0; i < myJson.end; i++){
		printk("%d,", myJson.frame[i]);
	}
	printk("<---end\r\n");
}

void ble_write_thread(void)
{
	/* Don't go any further until BLE is initialized */
	k_sem_take(&ble_init_ok, K_FOREVER);
// test3
	for (;;) {
		/* Wait indefinitely for data to be sent over bluetooth */
		struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data,
						     K_FOREVER);

		// LOG_INF("ble_write_thread: %d", buf->len);
/*
		printk("ble_write_thread: %d\r\n", buf->len);
		printk("buf: %s\r\n", buf->data);
*/
		// printk("---->%s\r\n", buf->data);
		// LOG_INF("---->%s", buf->data);
		getJsonData(buf->data, buf->len);	
		k_free(buf);
	}
	LOG_INF("ble_write_thread");
}

K_THREAD_DEFINE(ble_write_thread_id, STACKSIZE, ble_write_thread, NULL, NULL,
		NULL, PRIORITY, 0, 0);
