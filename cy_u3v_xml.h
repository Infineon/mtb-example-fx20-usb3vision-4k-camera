/***************************************************************************/
/*
 * \file cy_u3v_xml.h
 * \version 1.0
 *
 * Define macros and arrays for the U3V XML
 *
 *******************************************************************************
 * \copyright
 * (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************/

#ifndef CY_U3V_XML_H
#define CY_U3V_XML_H
#if defined(__cplusplus)
extern "C"
{
#endif
#define XML_FILE_LEN (11054)
#define XML_SHA1_LEN (20)

    extern const unsigned char glU3vXML[XML_FILE_LEN];
    extern const unsigned char glU3vXMLSha1[XML_SHA1_LEN];
#if defined(__cplusplus)
}
#endif

#endif /* CY_U3V_XML_H */
