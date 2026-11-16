#include "display.h"
#include "runtime.h"
#include "backlight.h"
#include <assert.h>
#include <string.h>
/* Fri 2026-11-06 local 23:00, UTC+8. */
#define FRIDAY_23 1793977200u
static screen_scene layout;
static void step(display_state *s, uint32_t ms, uint32_t utc, bool battery,
                  unsigned *page, bool wake) {
  display_step(s, ms, utc, 480, battery, 20, &layout, page, wake, wake, false);
}
int main(void) {
  scene_defaults(&layout);
  for(unsigned i=0;i<layout.pages;i++)layout.page[i].enabled=i<6;
  display_policy p; display_policy_default(&p);
  p.idle_brightness = 5; p.idle_page = 0;
  display_state s = {0}; unsigned page = 2;
  display_configure(&s, &p, 0xffff0000u);
  step(&s, 0xffff0000u + 119999u, 0, false, &page, false);
  assert(!s.sleeping);
  step(&s, 0xffff0000u + 120000u, 0, false, &page, false);
  assert(s.sleeping && s.reason == 1 && s.target == 5 && page == 0);
  step(&s, 0xffff0000u + 120001u, 0, false, &page, true);
  assert(!s.sleeping && page == 2 && s.target == 20);
  display_configure(&s, &p, 0);
  step(&s, 60000, 0, true, &page, false);
  assert(s.sleeping); /* Battery timeout is independent. */
  p.external_minutes = 0;
  p.count = 2;
  p.plans[0] = (display_plan){true, 1u << 4, 8, 1, 1380, 420, "night"};
  p.plans[1] = (display_plan){true, 127, 3, 0, 0, 1439, "lower priority"};
  uint8_t wire[DISPLAY_POLICY_BYTES];
  assert(display_policy_encode(&p, wire, sizeof(wire)));
  display_policy copy;
  assert(display_policy_decode(&copy, wire, sizeof(wire)));
  assert(!memcmp(&copy, &p, sizeof(p)));
  wire[16 + 5] = 255;
  assert(!display_policy_decode(&copy, wire, sizeof(wire)));
  memset(&s, 0, sizeof(s)); display_configure(&s, &p, 0); page = 2;
  step(&s, 0, FRIDAY_23, false, &page, false);
  assert(s.sleeping && s.target == 8 && page == 1);
  step(&s, 1, FRIDAY_23, false, &page, true);
  assert(!s.sleeping && page == 2);
  step(&s, 60000, FRIDAY_23 + 60, false, &page, false);
  assert(!s.sleeping);
  step(&s, 60001, FRIDAY_23 + 60, false, &page, false);
  assert(s.sleeping && s.target == 8); /* Never-idle wake override lasts 1 min. */
  step(&s, 70000, FRIDAY_23 + 7 * 3600, false, &page, false);
  assert(s.sleeping && s.target == 8); /* Saturday uses Friday's weekday. */
  step(&s, 80000, FRIDAY_23 + 8 * 3600, false, &page, false);
  assert(s.sleeping && s.target == 3 && page == 0); /* End exclusive. */
  p.count = 1; display_configure(&s, &p, 90000);
  step(&s, 90000, FRIDAY_23 + 8 * 3600, false, &page, false);
  assert(!s.sleeping && page == 2);
  step(&s, 90001, FRIDAY_23 + 7 * 86400, false, &page, false);
  assert(s.sleeping && s.target == 8); /* Next occurrence ignores last week's wake. */
  unsigned saved = page;
  display_step(&s, 100000, FRIDAY_23 + 8 * 3600, 480, false, 20, &layout,
                &page, true, true, true);
  assert(s.sleeping && page == saved); /* OTA wins. */
  p.plans[0].days = 0; assert(!display_policy_valid(&p));
  p.plans[0].enabled = false; assert(display_policy_valid(&p));
  /* V1 remains a global policy and resets new fields on decode. */
  uint8_t legacy[DISPLAY_POLICY_V1_BYTES];
  assert(display_policy_encode(&p,legacy,sizeof(legacy))&&legacy[1]==1);
  copy.home_page_id=99;copy.page_count=1;
  assert(display_policy_decode(&copy,legacy,sizeof(legacy))&&!copy.home_page_id&&!copy.page_count);
  display_policy_default(&p);p.home_page_id=layout.page[2].id;p.page_count=1;
  p.page_overrides[0]=(display_page_override){layout.page[2].id,1,3,9};
  assert(display_policy_encode(&p,wire,sizeof(wire))&&wire[1]==2);
  assert(display_policy_decode(&copy,wire,sizeof(wire))&&copy.home_page_id==p.home_page_id&&copy.page_count==1);
  assert(copy.page_overrides[0].idle_brightness==9);
  p.page_count=2;p.page_overrides[1]=p.page_overrides[0];assert(!display_policy_valid(&p));p.page_count=1;
  p.idle_page=0;memset(&s,0,sizeof(s));display_configure(&s,&p,0);page=2;
  step(&s,60000,0,false,&page,false);assert(s.sleeping&&page==0&&s.target==9);
  step(&s,60001,0,false,&page,false);assert(s.sleeping&&s.target==9); /* Destination inherits global, source remains override. */
  scene_page swap=layout.page[1];layout.page[1]=layout.page[2];layout.page[2]=swap;
  step(&s,60002,0,false,&page,false);assert(s.sleeping&&s.target==9);
  step(&s,60003,0,false,&page,true);assert(!s.sleeping&&page==1); /* Saved stable ID follows reorder. */
  memset(&s,0,sizeof(s));display_configure(&s,&p,0);page=1;
  step(&s,179999,0,true,&page,false);assert(!s.sleeping);
  step(&s,180000,0,true,&page,false);assert(s.sleeping&&s.target==9);
  layout.page[1].enabled=false;step(&s,180001,0,true,&page,true);assert(!s.sleeping&&page==0);
  /* A page without an entry inherits global, while zero idle never disables weekly plans. */
  memset(&s,0,sizeof(s));display_configure(&s,&p,0);page=3;
  step(&s,119999,0,false,&page,false);assert(!s.sleeping);
  step(&s,120000,0,false,&page,false);assert(s.sleeping&&s.target==0);
  layout.page[1].enabled=true;p.page_overrides[0].external_minutes=0;
  p.count=1;p.plans[0]=(display_plan){true,1u<<4,7,DISPLAY_KEEP_PAGE,1380,420,"night"};
  memset(&s,0,sizeof(s));display_configure(&s,&p,0);page=1;
  step(&s,900000,0,false,&page,false);assert(!s.sleeping);
  step(&s,900001,FRIDAY_23,false,&page,false);assert(s.sleeping&&s.target==7&&s.reason==2);
  /* Exercise the real negotiated runtime boundary, including downgrade. */
  static screen_runtime runtime;
  screen_runtime_init(&runtime);
  runtime.caps=(screen_caps){123,SCREEN_CAP_DISPLAY,true};
  char framed[1024];
  p.home_page_id=layout.page[1].id;
  assert(display_policy_encode(&p,wire,sizeof(wire)));
  size_t bytes=screen_link_encode(framed,sizeof(framed),123,1,wire,sizeof(wire));assert(bytes);
  screen_runtime_feed(&runtime,framed,bytes,10);
  assert(!runtime.display.configured&&!runtime.model.home_page_id); /* No V2 capability. */
  runtime.caps.capabilities|=SCREEN_CAP_PAGE_POLICY;
  bytes=screen_link_encode(framed,sizeof(framed),123,2,wire,sizeof(wire));
  screen_runtime_feed(&runtime,framed,bytes,20);
  assert(runtime.display.configured&&runtime.model.home_page_id==p.home_page_id&&runtime.display.policy.page_count==1);
  assert(display_policy_encode(&p,legacy,sizeof(legacy)));
  bytes=screen_link_encode(framed,sizeof(framed),123,3,legacy,sizeof(legacy));
  screen_runtime_feed(&runtime,framed,bytes,30);
  assert(runtime.display.configured&&!runtime.model.home_page_id&&!runtime.display.policy.page_count);
  return 0;
}
