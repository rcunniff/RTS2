/* 
 * JavaScript libraries for AJAX web access.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "libjavascript.h"
#include "../utils/utilsfunc.h"
#include "../utils/error.h"

using namespace rts2xmlrpc;

static const char *equScript = 
"function ln_range_degrees (deg) {\n"
  "var temp;\n"
    
  "if (angle >= 0.0 && angle < 360.0)\n"
    "return angle;\n"
 
  "temp = Math.floor(angle / 360);\n"
  "if (angle < 0.0)\n"
    "temp = temp - 1;\n"
  "temp = temp * 360;\n"
  "return angle - temp;\n"
"}\n"

"function ln_get_julian_from_sys() {\n"
  "var ld = new Date();\n"
  /* check for month = January or February */
  "var lY = ld.getUTCFullYear();\n"
  "var lM = ld.getUTCMonth();\n"
  "var lD = ld.getUTCDay();\n"

  "if (ld.getUTCMonth() < 2 ) {\n"
    "lY--;\n"
    "lM += 12;\n"
  "}\n"

  "a = lY / 100;\n"

  /* check for Julian or Gregorian calendar (starts Oct 4th 1582) */
  "if (lY > 1582 || (lY == 1582 && (lM > 10 || (lM == 10 && lD >= 4)))) {\n"
  /* Gregorian calendar */    
    "b = 2 - a + (a / 4);\n"
  "} else {\n"
  /* Julian calendar */
    "b = 0;\n"
  "}\n"
	
  /* add a fraction of hours, minutes and secs to days*/
  "days = lD + ld.getUTCHours() / 24.0 + ld.getUTCMinutes() / 1440.0 + ld.getUTCSeconds() /  86400.0;\n"

  /* now get the JD */
  "JD = Math.floor(365.25 * (lY + 4716)) + Math.floor(30.6001 * (lM + 1)) + days + b - 1524.5;\n"

  "return JD;\n"
"}\n"

"function ln_get_mean_sidereal_time (JD) {\n"
  "var sidereal;\n"
  "var T;\n"

  /* calc mean angle */
  "T = (JD - 2451545.0) / 36525.0;\n"

  "sidereal = 280.46061837 + (360.98564736629 * (JD - 2451545.0)) + (0.000387933 * T * T) - (T * T * T / 38710000.0);\n"
  /* add a convenient multiple of 360 degrees */
    
  "sidereal = ln_range_degrees (sidereal);\n"
    
   /* change to hours */
  "return sidereal * 24.0 / 360.0;\n"
"}\n"

"function AltAz () {\n"
  "this.alt = Infinity;\n"
  "this.az = Infinity;\n"
"}\n"

"function LnLat (ln, lat) {\n"
  "this.ln = ln;\n"
  "this.lat = lat;\n"
"}\n"

"function RaDec (ra, dec) {\n"
  "this.ra = ra;\n"
  "this.dec = dec;\n"
  "this.altaz = function (altaz) {\n"
    "sidereal = ln_get_mean_sidereal_time (ln_get_julian_from_sys());\n"
    "ln_get_hrz_from_equ_sidereal_time (this, LnLat(15,30), sidereal, altaz);\n"
  "}\n"
"}\n";

static const char *dateScript = 
"function positionInfo(object) {\n"
"\n"
"  var p_elm = object;\n"
"\n"
"  this.getElementLeft = getElementLeft;\n"
"  function getElementLeft() {\n"
"    var x = 0;\n"
"    var elm;\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    while (elm != null) {\n"
"      if(elm.style.position == 'relative') {\n"
"        break;\n"
"      }\n"
"      else {\n"
"        x += elm.offsetLeft;\n"
"        elm = elm.offsetParent;\n"
"      }\n"
"    }\n"
"    return parseInt(x);\n"
"  }\n"
"\n"
"  this.getElementWidth = getElementWidth;\n"
"  function getElementWidth(){\n"
"    var elm;\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    return parseInt(elm.offsetWidth);\n"
"  }\n"
"\n"
"  this.getElementRight = getElementRight;\n"
"  function getElementRight(){\n"
"    return getElementLeft(p_elm) + getElementWidth(p_elm);\n"
"  }\n"
"\n"
"  this.getElementTop = getElementTop;\n"
"  function getElementTop() {\n"
"    var y = 0;\n"
"    var elm;\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    while (elm != null) {\n"
"      if(elm.style.position == 'relative') {\n"
"        break;\n"
"      }\n"
"      else {\n"
"        y+= elm.offsetTop;\n"
"        elm = elm.offsetParent;\n"
"      }\n"
"    }\n"
"    return parseInt(y);\n"
"  }\n"
"\n"
"  this.getElementHeight = getElementHeight;\n"
"  function getElementHeight(){\n"
"    var elm;\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    return parseInt(elm.offsetHeight);\n"
"  }\n"
"\n"
"  this.getElementBottom = getElementBottom;\n"
"  function getElementBottom(){\n"
"    return getElementTop(p_elm) + getElementHeight(p_elm);\n"
"  }\n"
"}\n"
"\n"
"function CalendarControl() {\n"
"\n"
"  var calendarId = 'CalendarControl';\n"
"  var currentYear = 0;\n"
"  var currentMonth = 0;\n"
"  var currentDay = 0;\n"
"\n"
"  var selectedYear = 0;\n"
"  var selectedMonth = 0;\n"
"  var selectedDay = 0;\n"
"\n"
"  var months = ['January','February','March','April','May','June','July','August','September','October','November','December'];\n"
"  var dateField = null;\n"
"\n"
"  function getProperty(p_property){\n"
"    var p_elm = calendarId;\n"
"    var elm = null;\n"
"\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    if (elm != null){\n"
"      if(elm.style){\n"
"        elm = elm.style;\n"
"        if(elm[p_property]){\n"
"          return elm[p_property];\n"
"        } else {\n"
"          return null;\n"
"        }\n"
"      } else {\n"
"        return null;\n"
"      }\n"
"    }\n"
"  }\n"
"\n"
"  function setElementProperty(p_property, p_value, p_elmId){\n"
"    var p_elm = p_elmId;\n"
"    var elm = null;\n"
"\n"
"    if(typeof(p_elm) == \"object\"){\n"
"      elm = p_elm;\n"
"    } else {\n"
"      elm = document.getElementById(p_elm);\n"
"    }\n"
"    if((elm != null) && (elm.style != null)){\n"
"      elm = elm.style;\n"
"      elm[ p_property ] = p_value;\n"
"    }\n"
"  }\n"
"\n"
"  function setProperty(p_property, p_value) {\n"
"    setElementProperty(p_property, p_value, calendarId);\n"
"  }\n"
"\n"
"  function getDaysInMonth(year, month) {\n"
"    return [31,((!(year % 4 ) && ( (year % 100 ) || !( year % 400 ) ))?29:28),31,30,31,30,31,31,30,31,30,31][month-1];\n"
"  }\n"
"\n"
"  function getDayOfWeek(year, month, day) {\n"
"    var date = new Date(year,month-1,day)\n"
"    return date.getDay();\n"
"  }\n"
"\n"
"  this.clearDate = clearDate;\n"
"  function clearDate() {\n"
"    dateField.value = '';\n"
"    hide();\n"
"  }\n"
"\n"
"  this.setDate = setDate;\n"
"  function setDate(year, month, day) {\n"
"    if (dateField) {\n"
"      if (month < 10) {month = \"0\" + month;}\n"
"      if (day < 10) {day = \"0\" + day;}\n"
"\n"
"      var dateString = year+'/'+month+'/'+day;\n"
"      dateField.value = dateString;\n"
"      hide();\n"
"    }\n"
"    return;\n"
"  }\n"
"\n"
"  this.changeMonth = changeMonth;\n"
"  function changeMonth(change) {\n"
"    currentMonth += change;\n"
"    currentDay = 0;\n"
"    if(currentMonth > 12) {\n"
"      currentMonth = 1;\n"
"      currentYear++;\n"
"    } else if(currentMonth < 1) {\n"
"      currentMonth = 12;\n"
"      currentYear--;\n"
"    }\n"
"\n"
"    calendar = document.getElementById(calendarId);\n"
"    calendar.innerHTML = calendarDrawTable();\n"
"  }\n"
"\n"
"  this.changeYear = changeYear;\n"
"  function changeYear(change) {\n"
"    currentYear += change;\n"
"    currentDay = 0;\n"
"    calendar = document.getElementById(calendarId);\n"
"    calendar.innerHTML = calendarDrawTable();\n"
"  }\n"
"\n"
"  function getCurrentYear() {\n"
"    var year = new Date().getYear();\n"
"    if(year < 1900) year += 1900;\n"
"    return year;\n"
"  }\n"
"\n"
"  function getCurrentMonth() {\n"
"    return new Date().getMonth() + 1;\n"
"  } \n"
"\n"
"  function getCurrentDay() {\n"
"    return new Date().getDate();\n"
"  }\n"
"\n"
"  function calendarDrawTable() {\n"
"\n"
"    var dayOfMonth = 1;\n"
"    var validDay = 0;\n"
"    var startDayOfWeek = getDayOfWeek(currentYear, currentMonth, dayOfMonth);\n"
"    var daysInMonth = getDaysInMonth(currentYear, currentMonth);\n"
"    var css_class = null; //CSS class for each day\n"
"\n"
"    var table = \"<table cellspacing='0' cellpadding='0' border='0'>\";\n"
"    table = table + \"<tr class='header'>\";\n"
"    table = table + \"  <td colspan='2' class='previous'><a href='javascript:changeCalendarControlMonth(-1);'>&lt;</a> <a href='javascript:changeCalendarControlYear(-1);'>&laquo;</a></td>\";\n"
"    table = table + \"  <td colspan='3' class='title'>\" + months[currentMonth-1] + \"<br>\" + currentYear + \"</td>\";\n"
"    table = table + \"  <td colspan='2' class='next'><a href='javascript:changeCalendarControlYear(1);'>&raquo;</a> <a href='javascript:changeCalendarControlMonth(1);'>&gt;</a></td>\";\n"
"    table = table + \"</tr>\";\n"
"    table = table + \"<tr><th>S</th><th>M</th><th>T</th><th>W</th><th>T</th><th>F</th><th>S</th></tr>\";\n"
"\n"
"    for(var week=0; week < 6; week++) {\n"
"      table = table + \"<tr>\";\n"
"      for(var dayOfWeek=0; dayOfWeek < 7; dayOfWeek++) {\n"
"        if(week == 0 && startDayOfWeek == dayOfWeek) {\n"
"          validDay = 1;\n"
"        } else if (validDay == 1 && dayOfMonth > daysInMonth) {\n"
"          validDay = 0;\n"
"        }\n"
"\n"
"        if(validDay) {\n"
"          if (dayOfMonth == selectedDay && currentYear == selectedYear && currentMonth == selectedMonth) {\n"
"            css_class = 'current';\n"
"          } else if (dayOfWeek == 0 || dayOfWeek == 6) {\n"
"            css_class = 'weekend';\n"
"          } else {\n"
"            css_class = 'weekday';\n"
"          }\n"
"\n"
"          table = table + \"<td><a class='\"+css_class+\"' href=\\\"javascript:setCalendarControlDate(\"+currentYear+\",\"+currentMonth+\",\"+dayOfMonth+\")\\\">\"+dayOfMonth+\"</a></td>\";\n"
"          dayOfMonth++;\n"
"        } else {\n"
"          table = table + \"<td class='empty'>&nbsp;</td>\";\n"
"        }\n"
"      }\n"
"      table = table + \"</tr>\";\n"
"    }\n"
"\n"
"    table = table + \"<tr class='header'><th colspan='7' style='padding: 3px;'><a href='javascript:clearCalendarControl();'>Clear</a> | <a href='javascript:hideCalendarControl();'>Close</a></td></tr>\";\n"
"    table = table + \"</table>\";\n"
"\n"
"    return table;\n"
"  }\n"
"\n"
"  this.show = show;\n"
"  function show(field) {\n"
"    can_hide = 0;\n"
"  \n"
"    // If the calendar is visible and associated with\n"
"    // this field do not do anything.\n"
"    if (dateField == field) {\n"
"      return;\n"
"    } else {\n"
"      dateField = field;\n"
"    }\n"
"\n"
"    if(dateField) {\n"
"      try {\n"
"        var dateString = new String(dateField.value);\n"
"        var dateParts = dateString.split('/');\n"
"        \n"
"        selectedYear = parseInt(dateParts[0],10);\n"
"        selectedMonth = parseInt(dateParts[1],10);\n"
"        selectedDay = parseInt(dateParts[2],10);\n"
"      } catch(e) {}\n"
"    }\n"
"\n"
"    if (!(selectedYear && selectedMonth && selectedDay)) {\n"
"      selectedMonth = getCurrentMonth();\n"
"      selectedDay = getCurrentDay();\n"
"      selectedYear = getCurrentYear();\n"
"    }\n"
"\n"
"    currentMonth = selectedMonth;\n"
"    currentDay = selectedDay;\n"
"    currentYear = selectedYear;\n"
"\n"
"    if(document.getElementById){\n"
"\n"
"      calendar = document.getElementById(calendarId);\n"
"      calendar.innerHTML = calendarDrawTable(currentYear, currentMonth);\n"
"\n"
"      setProperty('display', 'block');\n"
"\n"
"      var fieldPos = new positionInfo(dateField);\n"
"      var calendarPos = new positionInfo(calendarId);\n"
"\n"
"      var x = fieldPos.getElementLeft();\n"
"      var y = fieldPos.getElementBottom();\n"
"\n"
"      setProperty('left', x + \"px\");\n"
"      setProperty('top', y + \"px\");\n"
" \n"
"      if (document.all) {\n"
"        setElementProperty('display', 'block', 'CalendarControlIFrame');\n"
"        setElementProperty('left', x + \"px\", 'CalendarControlIFrame');\n"
"        setElementProperty('top', y + \"px\", 'CalendarControlIFrame');\n"
"        setElementProperty('width', calendarPos.getElementWidth() + \"px\", 'CalendarControlIFrame');\n"
"        setElementProperty('height', calendarPos.getElementHeight() + \"px\", 'CalendarControlIFrame');\n"
"      }\n"
"    }\n"
"  }\n"
"\n"
"  this.hide = hide;\n"
"  function hide() {\n"
"    if(dateField) {\n"
"      setProperty('display', 'none');\n"
"      setElementProperty('display', 'none', 'CalendarControlIFrame');\n"
"      dateField = null;\n"
"    }\n"
"  }\n"
"\n"
"  this.visible = visible;\n"
"  function visible() {\n"
"    return dateField\n"
"  }\n"
"\n"
"  this.can_hide = can_hide;\n"
"  var can_hide = 0;\n"
"}\n"
"\n"
"var calendarControl = new CalendarControl();\n"
"\n"
"function showCalendarControl(textField) {\n"
"  // textField.onblur = hideCalendarControl;\n"
"  calendarControl.show(textField);\n"
"}\n"
"\n"
"function clearCalendarControl() {\n"
"  calendarControl.clearDate();\n"
"}\n"
"\n"
"function hideCalendarControl() {\n"
"  if (calendarControl.visible()) {\n"
"    calendarControl.hide();\n"
"  }\n"
"}\n"
"\n"
"function setCalendarControlDate(year, month, day) {\n"
"  calendarControl.setDate(year, month, day);\n"
"}\n"
"\n"
"function changeCalendarControlYear(change) {\n"
"  calendarControl.changeYear(change);\n"
"}\n"
"\n"
"function changeCalendarControlMonth(change) {\n"
"  calendarControl.changeMonth(change);\n"
"}\n"
"\n"
"document.write(\"<iframe id='CalendarControlIFrame' src='javascript:false;' frameBorder='0' scrolling='no'></iframe>\");\n"
"document.write(\"<div id='CalendarControl'></div>\");\n";

void LibJavaScript::authorizedExecute (std::string path, XmlRpc::HttpParams *params, const char* &response_type, char* &response, size_t &response_length)
{
	const char *reply = NULL;
	
	std::vector <std::string> vals = SplitStr (path, std::string ("/"));
	if (vals.size () != 1)
		throw rts2core::Error ("File not found");

	if (vals[0] == "equ.js")
		reply = equScript;
	else if (vals[0] == "date.js")
		reply = dateScript;
	else 
		throw rts2core::Error ("JavaScript not found");

	response_length = strlen (reply);
	response = new char[response_length];
	response_type = "text/javascript";
	memcpy (response, reply, response_length);
}
