#include "ws.h"
#include "functionBle.h"

struct async_resp_arg
{
  httpd_handle_t hd; /// HTTP Server Instance Handle
  int fd;            /// The file descriptor for this socket
};

static struct async_resp_arg resp_arg;
static bool _is_connected = false;
static void (*_transportEvent) (int msg_id, ...) = NULL;

void
ws_init (void (*cb) (int msg_id, ...))
{
  _transportEvent = cb;
  ESP_LOGI ("", "%s:%d _transportEvent:%x", __FILE__, __LINE__,
            _transportEvent);
}

esp_err_t
ws_handler (httpd_req_t *req)
{
  if (req->method == HTTP_GET)
    {
      resp_arg.hd = req->handle;
      resp_arg.fd = httpd_req_to_sockfd (req);
      _is_connected = true;
      ESP_LOGI ("",
                "%s:%d: Handshake done, the new connection opened: sock.hd: "
                "%x, sock.fd: %d",
                __FILE__, __LINE__, resp_arg.hd, resp_arg.fd);
      if (NULL != _transportEvent)
        _transportEvent (PeerConnected, resp_arg.fd);
      return ESP_OK;
    }

  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset (&ws_pkt, 0, sizeof (httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame (req, &ws_pkt, 0);
  if (ret != ESP_OK)
    {
      ESP_LOGE (__FILE__,
                "httpd_ws_recv_frame failed to get frame len with %d", ret);
      return ret;
    }
  ESP_LOGI ("", "%s:%d: frame len is %d", __FILE__, __LINE__, ws_pkt.len);
  if (ws_pkt.len)
    {
      buf = calloc (1, ws_pkt.len * 2);
      if (buf == NULL)
        {
          ESP_LOGE ("", "%s:%d: Failed to calloc memory for buf", __FILE__,
                    __LINE__);
          return ESP_ERR_NO_MEM;
        }
      ws_pkt.payload = buf;
      ret = httpd_ws_recv_frame (req, &ws_pkt, ws_pkt.len);
      if (ret == ESP_OK)
        {
          ESP_LOGI ("",
                    "%s:%d: Got packet with message: %s _transportEvent: %x",
                    __FILE__, __LINE__, ws_pkt.payload, _transportEvent);
          if (_transportEvent)
            _transportEvent (DrvDataReceived, resp_arg.fd, ws_pkt.payload,
                             ws_pkt.len);
        }
      free (buf);
    }
  return ret;
}

size_t
ws_send_async (int id, char *const data, const size_t length)
{
  if (!_is_connected)
    {
      ESP_LOGE ("", "%s:%d ERROR: No connection available!", __FILE__, __LINE__);
      return length;
    }
  httpd_ws_frame_t ws_pkt = { 0 };
  ws_pkt.payload = (uint8_t *)data;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.len = length;
  ESP_LOGD ("", "%s:%d resp_arg.hd: %x,  resp_arg.fd: %d", __FILE__, __LINE__,
            resp_arg.hd, resp_arg.fd);
  ESP_LOGD ("", "%s:%d data: %s length: %d", __FILE__, __LINE__,
            ws_pkt.payload, ws_pkt.len);
  return (ESP_OK
          == httpd_ws_send_frame_async (resp_arg.hd, resp_arg.fd, &ws_pkt))
             ? ws_pkt.len
             : 0;
}