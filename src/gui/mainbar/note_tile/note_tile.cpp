/****************************************************************************
 *   Tu May 22 21:23:51 2020
 *   Copyright  2020  Dirk Brosswick
 *   Email: dirk.brosswick@googlemail.com
 ****************************************************************************/
 
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "hardware/wifictl.h"
#include "gui/mainbar/mainbar.h"
#include "note_tile.h"
#include "thouters_secrets.h"

static lv_obj_t *note_cont = NULL;


static lv_style_t *style;
//static lv_style_t notestyle;
typedef struct {
    char server[64] = THOUTERS_MQTT_SERVER;
    int32_t port = 1883;
    bool ssl = false;
    char user[32] = THOUTERS_MQTT_USER;
    char password[32] = THOUTERS_MQTT_PASSWORD;
    char topic[64] = THOUTERS_MQTT_TOPIC;
    bool autoconnect = true;
} mqtt_config_t;

mqtt_config_t mqtt_config;
/*
= 
{
    .server = "hass",
    .user = "mqtt",
    .password = "mqttpaardenbloem",
    .topic="twatch",
    .autoconnect =true
};
*/
lv_task_t * mqtt_main_task;
WiFiClient myespClient;
PubSubClient mqtt_client( myespClient );
LV_FONT_DECLARE(Ubuntu_72px);
LV_FONT_DECLARE(Ubuntu_16px);
//LV_IMG_DECLARE( coffee );
//LV_IMG_DECLARE( television_classic );
LV_IMG_DECLARE(book_open_page_variant);
LV_IMG_DECLARE(coffee);
LV_IMG_DECLARE(lightbulb_outline);
LV_IMG_DECLARE(television_classic);
LV_IMG_DECLARE(track_light);

#define NR_OF_OPTIONS 5
typedef struct {
    const void *image;
    char key[32];
} option_t;

option_t options[NR_OF_OPTIONS] =
{
    { &coffee, "Ontbijt"},
    { &track_light, "Maximum"}, 
    { &lightbulb_outline, "Uit"},
    { &book_open_page_variant, "Lezen"}, 
    { &television_classic, "TV"}
};
static lv_obj_t * imgbtns[NR_OF_OPTIONS] ;

bool mqtt_wifi_event_cb( EventBits_t event, void *arg ) 
{
    switch( event ) 
    {
        case WIFICTL_CONNECT_IP:
        if ( mqtt_config.autoconnect ) {
            mqtt_client.setServer( mqtt_config.server, mqtt_config.port);
            if ( !mqtt_client.connect( "powermeter", mqtt_config.user, mqtt_config.password ) ) {
                log_e("connect to mqtt server %s failed", mqtt_config.server );
//                app_set_indicator( powermeter_get_app_icon(), ICON_INDICATOR_FAIL );
//                widget_set_indicator( powermeter_get_widget_icon() , ICON_INDICATOR_FAIL );
            }
            else {
                log_i("connect to mqtt server %s success", mqtt_config.server );
                mqtt_client.subscribe( mqtt_config.topic );
 //               app_set_indicator( powermeter_get_app_icon(), ICON_INDICATOR_OK );
//                widget_set_indicator( powermeter_get_widget_icon(), ICON_INDICATOR_OK );
            }
        } 
        break;
        case WIFICTL_OFF_REQUEST:
        case WIFICTL_OFF:
        case WIFICTL_DISCONNECT:    
            log_i("disconnect from mqtt server %s", mqtt_config.server );
            mqtt_client.disconnect();
//            app_hide_indicator( powermeter_get_app_icon() );
//            widget_hide_indicator( powermeter_get_widget_icon() );
//            widget_set_label( powermeter_get_widget_icon(), "n/a" );
            break;
    }
    return( true );
}


void pubsubclient_callback(char* topic, byte* payload, unsigned int length) 
{
    bool updated=false;
    int updated_tilenr= 0;

    payload[length] = '\0';

    log_i("mqtt %s %s",topic,payload);
    if (0!= strcmp(mqtt_config.topic, topic))
    {
        return;
    }

    // when checked, set other options off
    for (int i = 0; i < NR_OF_OPTIONS; i++)
    {
        if(0==strcmp(options[i].key, (const char*)payload))
        {
            
            if ((lv_imgbtn_get_state(imgbtns[i]) & LV_STATE_CHECKED)!= LV_STATE_CHECKED)
            {
                updated = true;
                updated_tilenr = i;
            }

            log_i("cmp %s == %s",options[i].key,payload);
            lv_imgbtn_set_state(imgbtns[i], (lv_imgbtn_get_state(imgbtns[i]) | LV_STATE_CHECKED));
        }
        else
        {
            log_i("cmp %s != %s",options[i].key,payload);
            lv_imgbtn_set_state(imgbtns[i], LV_STATE_DEFAULT);
        }
    }
    if (updated)
    {
        mainbar_jump_to_tilenumber(updated_tilenr, LV_ANIM_ON );
    }

}
static void btn_event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) 
    {
        lv_btn_state_t new_state = lv_imgbtn_get_state(obj);
        if ((new_state & LV_STATE_CHECKED) == LV_STATE_CHECKED)
        {
            // when checked, set other options off
            int new_option;
            bool updated = false;
            for (int i = 0; i < NR_OF_OPTIONS; i++)
            {
                if(imgbtns[i] != obj)
                {
                    
                    log_i("clearing %d",i);
                    lv_imgbtn_set_state(imgbtns[i], LV_STATE_DEFAULT);
                }
                else
                {
                    log_i("enabled %d",i);
                    new_option = i;
                    updated = true;
                }
            }
            // if connected notify of update
            //
            if (updated)
            {
                char * payload = options[new_option].key;
                log_i("publish %s %s",mqtt_config.topic,payload);
                mqtt_client.publish(mqtt_config.topic, payload, strlen(payload));
            }
        }
    }
}

void mqtt_main_task_cb( lv_task_t * task ) 
{
    // put your code her
    mqtt_client.loop();
}
void note_tile_setup( void ) 
{
    style = mainbar_get_style();

    static lv_style_t imgstyle;
    lv_style_init(&imgstyle);
    lv_style_set_image_recolor_opa(&imgstyle, LV_STATE_CHECKED, LV_OPA_100);
    lv_style_set_image_recolor(&imgstyle, LV_STATE_CHECKED, LV_COLOR_LIME);

    for (int i=0; i < NR_OF_OPTIONS; i++)
    {
        lv_obj_t * imgbtn;
        const void *img = options[i].image;

        note_cont = mainbar_get_tile_obj( mainbar_add_tile( 0, i, "note tile" ) );
        imgbtn = lv_imgbtn_create(note_cont, NULL);
        lv_imgbtn_set_src(imgbtn, LV_BTN_STATE_RELEASED, img);
        lv_imgbtn_set_src(imgbtn, LV_BTN_STATE_PRESSED, img);
        lv_imgbtn_set_src(imgbtn, LV_BTN_STATE_CHECKED_RELEASED, img);
        lv_imgbtn_set_src(imgbtn, LV_BTN_STATE_CHECKED_PRESSED, img);
        lv_imgbtn_set_checkable(imgbtn, true);
        lv_obj_add_style(imgbtn, LV_IMGBTN_PART_MAIN, &imgstyle);
        lv_obj_align(imgbtn, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_event_cb(imgbtn, btn_event_handler);

        mainbar_add_slide_element(imgbtn);
        imgbtns[i]= imgbtn;
    }

    mqtt_client.setCallback( pubsubclient_callback );
    mqtt_client.setBufferSize( 512 );


    wifictl_register_cb( WIFICTL_CONNECT_IP | WIFICTL_OFF_REQUEST | WIFICTL_OFF | WIFICTL_DISCONNECT , mqtt_wifi_event_cb, "powermeter" );

    mqtt_main_task = lv_task_create( mqtt_main_task_cb, 250, LV_TASK_PRIO_MID, NULL );
}

