/***************************************************************************
 *
 * Project:  OpenCPN Weather Routing plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2014 by Sean D'Epagnier                                 *
 *   sean@depagnier.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */

#include <wx/wx.h>

#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "tinyxml/tinyxml.h"

#include "Utilities.h"
#include "Boat.h"
#include "RouteMap.h"
#include "ConfigurationDialog.h"
#include "BoatDialog.h"
#include "weather_routing_pi.h"
#include "WeatherRouting.h"

ConfigurationDialog::ConfigurationDialog(WeatherRouting &weatherrouting)
    : ConfigurationDialogBase(&weatherrouting), m_WeatherRouting(weatherrouting)
{
    wxFileConfig *pConf = GetOCPNConfigObject();
    pConf->SetPath ( _T( "/PlugIns/WeatherRouting" ) );

    wxPoint p = GetPosition();
    pConf->Read ( _T ( "ConfigurationX" ), &p.x, p.x);
    pConf->Read ( _T ( "ConfigurationY" ), &p.y, p.y);
    SetPosition(p);
}

ConfigurationDialog::~ConfigurationDialog( )
{
    wxFileConfig *pConf = GetOCPNConfigObject();
    pConf->SetPath ( _T( "/PlugIns/WeatherRouting" ) );

    wxPoint p = GetPosition();
    pConf->Write ( _T ( "ConfigurationX" ), p.x);
    pConf->Write ( _T ( "ConfigurationY" ), p.y);
}

void ConfigurationDialog::EditBoat( )
{
    BoatDialog boatdlg(this, m_fpBoat->GetPath());
    int updated = boatdlg.ShowModal();
    m_fpBoat->SetPath(boatdlg.m_boatpath);

    if(updated == wxID_OK) {
        /* update any configurations that use this boat */
        m_WeatherRouting.UpdateBoatFilename(Configuration());
        Update();
    }
}

void ConfigurationDialog::OnGribTime( wxCommandEvent& event )
{
    SetStartDateTime(m_GribTimelineTime);
    Update();
}

void ConfigurationDialog::OnCurrentTime( wxCommandEvent& event )
{
    SetStartDateTime(wxDateTime::Now());
    Update();
}

void ConfigurationDialog::OnAvoidCyclones( wxCommandEvent& event )
{
    m_sCycloneMonths->Enable(event.IsChecked());
    m_sCycloneDays->Enable(event.IsChecked());
    m_sCycloneWindSpeed->Enable(event.IsChecked());
    m_sCycloneClimatologyStartYear->Enable(event.IsChecked());
    Update();
}

void ConfigurationDialog::OnAddDegreeStep( wxCommandEvent& event )
{
    int s = m_lDegreeSteps->GetSelection();
    if(s == -1)
        s = m_lDegreeSteps->GetCount();

    double step;
    m_tDegreeStep->GetValue().ToDouble(&step);
    m_lDegreeSteps->Insert(wxString::Format(_T("%f"), step), s);
    m_tDegreeStep->Clear();
    Update();
}

void ConfigurationDialog::OnRemoveDegreeStep( wxCommandEvent& event )
{
    int s = m_lDegreeSteps->GetSelection();
    if(s == -1)
        return;

    m_lDegreeSteps->Delete(s);
    m_lDegreeSteps->SetSelection(s);
    Update();
}

void ConfigurationDialog::OnClearDegreeSteps( wxCommandEvent& event )
{
    m_lDegreeSteps->Clear();
    Update();
}

void ConfigurationDialog::OnGenerateDegreeSteps( wxCommandEvent& event )
{
    RouteMapConfiguration configuration = Configuration();
    GenerateDegreeSteps(configuration);
    SetConfiguration(configuration);
    m_WeatherRouting.SetConfigurationCurrentRoute(configuration);
}

void ConfigurationDialog::GenerateDegreeSteps(RouteMapConfiguration &configuration)
{
    double from, to, by;
    m_tFromDegrees->GetValue().ToDouble(&from);
    m_tToDegrees->GetValue().ToDouble(&to);
    m_tByDegrees->GetValue().ToDouble(&by);

    if(from < 0 || from >= 180 || to <= 0 || to > 180 || from >= to || by <= 0 || by >= 180) {
        wxMessageDialog mdlg(this, _("Invalid settings, nothing will be done."),
                             wxString(_("Weather Routing"), wxOK | wxICON_WARNING));
        mdlg.ShowModal();
        return;
    }

    configuration.DegreeSteps.clear();
    for(double v = from; v <= to; v+=by) {
        configuration.DegreeSteps.push_back(v);
        configuration.DegreeSteps.push_back(-v);
    }
    configuration.DegreeSteps.sort();
}

void ConfigurationDialog::SetConfiguration(RouteMapConfiguration configuration)
{
    m_cStart->SetValue(configuration.Start);

    m_dpStartDate->SetValue(configuration.StartTime);
    m_tStartHour->SetValue(wxString::Format(_T("%.2f"),
                                            configuration.StartTime.GetHour()
                                            + (double)configuration.StartTime.GetMinute()/60.0));

    wxFileName path = configuration.boatFileName;
    m_fpBoat->SetPath(path.GetFullPath());

    int dt = configuration.dt;
    int hours = dt / 3600;
    m_sTimeStepHours->SetValue(wxString::Format(_T("%d"), hours));
    dt -= hours * 3600;
    int minutes = dt / 60;
    m_sTimeStepMinutes->SetValue(wxString::Format(_T("%d"), minutes));
    dt -= minutes * 60;
    m_sTimeStepSeconds->SetValue(wxString::Format(_T("%d"), dt));

    m_cEnd->SetValue(configuration.End);

    m_lDegreeSteps->Clear();
    for(std::list<double>::iterator it = configuration.DegreeSteps.begin();
        it != configuration.DegreeSteps.end(); it++)
        m_lDegreeSteps->Append(wxString::Format(_T("%.1f"), *it));

    m_rbNewton->SetValue(configuration.Integrator == RouteMapConfiguration::NEWTON);
    m_rbRungeKutta->SetValue(configuration.Integrator == RouteMapConfiguration::RUNGE_KUTTA);

    m_sMaxDivertedCourse->SetValue(configuration.MaxDivertedCourse);
    m_sMaxSearchAngle->SetValue(configuration.MaxSearchAngle);
    m_sMaxWindKnots->SetValue(configuration.MaxWindKnots);
    m_sMaxSwellMeters->SetValue(configuration.MaxSwellMeters);

    m_sMaxLatitude->SetValue(configuration.MaxLatitude);
    m_sMaxTacks->SetValue(configuration.MaxTacks);
    m_sTackingTime->SetValue(configuration.TackingTime);

    m_cAvoidCycloneTracks->SetValue(configuration.AvoidCycloneTracks);
    m_sCycloneMonths->SetValue(configuration.CycloneMonths);
    m_sCycloneDays->SetValue(configuration.CycloneDays);
    m_sCycloneWindSpeed->SetValue(configuration.CycloneWindSpeed);
    m_sCycloneClimatologyStartYear->SetValue(configuration.CycloneClimatologyStartYear);

    m_cbDetectLand->SetValue(configuration.DetectLand);
    m_cbCurrents->SetValue(configuration.Currents);
    m_cbInvertedRegions->SetValue(configuration.InvertedRegions);
    m_cbAnchoring->SetValue(configuration.Anchoring);

    m_cbAllowDataDeficient->SetValue(configuration.AllowDataDeficient);

    m_cbUseGrib->SetValue(configuration.UseGrib);
    m_cClimatologyType->SetSelection(configuration.ClimatologyType);
}

RouteMapConfiguration ConfigurationDialog::Configuration()
{
    RouteMapConfiguration configuration;

    configuration.Start = m_cStart->GetValue();

    configuration.StartTime = m_dpStartDate->GetValue();
    double hour;
    m_tStartHour->GetValue().ToDouble(&hour);
    configuration.StartTime.SetHour((int)hour);
    configuration.StartTime.SetMinute((int)(60*hour)%60);

    configuration.boatFileName = m_fpBoat->GetPath();

    configuration.dt = 60*(60*m_sTimeStepHours->GetValue()
                     + m_sTimeStepMinutes->GetValue())
        + m_sTimeStepSeconds->GetValue();
    if(configuration.dt == 0) {
        wxMessageDialog mdlg(this, _("Zero Time Step invalid"),
                             _("Weather Routing"), wxOK | wxICON_WARNING);
        mdlg.ShowModal();
    }

    configuration.End = m_cEnd->GetValue();

    if(m_lDegreeSteps->GetCount() < 4) {
        wxMessageDialog mdlg(this, _("Warning: less than 4 different degree steps specified\n"),
                             wxString(_("Weather Routing"), wxOK | wxICON_WARNING));
        mdlg.ShowModal();
    }

    configuration.DegreeSteps.clear();
    for(unsigned int i=0; i<m_lDegreeSteps->GetCount(); i++) {
        double step;
        m_lDegreeSteps->GetString(i).ToDouble(&step);
        configuration.DegreeSteps.push_back(positive_degrees(step));
    }
    configuration.DegreeSteps.sort();

    configuration.Integrator = m_rbRungeKutta->GetValue() ?
        RouteMapConfiguration::RUNGE_KUTTA : RouteMapConfiguration::NEWTON;

    configuration.MaxDivertedCourse = m_sMaxDivertedCourse->GetValue();
    configuration.MaxSearchAngle = m_sMaxSearchAngle->GetValue();
    configuration.MaxWindKnots = m_sMaxWindKnots->GetValue();
    configuration.MaxSwellMeters = m_sMaxSwellMeters->GetValue();

    configuration.MaxLatitude = m_sMaxLatitude->GetValue();
    configuration.MaxTacks = m_sMaxTacks->GetValue();
    configuration.TackingTime = m_sTackingTime->GetValue();

    configuration.AvoidCycloneTracks = m_cAvoidCycloneTracks->GetValue();
    configuration.CycloneMonths = m_sCycloneMonths->GetValue();
    configuration.CycloneDays = m_sCycloneDays->GetValue();
    configuration.CycloneWindSpeed = m_sCycloneWindSpeed->GetValue();
    configuration.CycloneClimatologyStartYear = m_sCycloneClimatologyStartYear->GetValue();

    configuration.DetectLand = m_cbDetectLand->GetValue();
    configuration.Currents = m_cbCurrents->GetValue();
    configuration.InvertedRegions = m_cbInvertedRegions->GetValue();
    configuration.Anchoring = m_cbAnchoring->GetValue();

    configuration.AllowDataDeficient = m_cbAllowDataDeficient->GetValue();

    configuration.UseGrib = m_cbUseGrib->GetValue();
    configuration.ClimatologyType = (RouteMapConfiguration::ClimatologyDataType)
        m_cClimatologyType->GetSelection();

    return configuration;
}

void ConfigurationDialog::AddSource(wxString name)
{
    m_cStart->Append(name);
    m_cEnd->Append(name);
}

void ConfigurationDialog::RemoveSource( wxString name )
{
    int i = m_cStart->FindString(name, true);
    if(i >= 0) {
        m_cStart->Delete(i);
        m_cEnd->Delete(i);
    }
}

void ConfigurationDialog::ClearSources()
{
    m_cStart->Clear();
    m_cEnd->Clear();
}

void ConfigurationDialog::SetStartDateTime(wxDateTime datetime)
{
    if(datetime.IsValid()) {
        m_dpStartDate->SetValue(datetime);
        m_tStartHour->SetValue(wxString::Format(_T("%.3f"), datetime.GetHour()
                                                +datetime.GetMinute() / 60.0));
    } else {
        wxMessageDialog mdlg(this, _("Invalid Date Time."),
                             wxString(_("Weather Routing"), wxOK | wxICON_WARNING));
        mdlg.ShowModal();
    }
}

void ConfigurationDialog::Update()
{
    m_WeatherRouting.SetConfigurationCurrentRoute(Configuration());
}
