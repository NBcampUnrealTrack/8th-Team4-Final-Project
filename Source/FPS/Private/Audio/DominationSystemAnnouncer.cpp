#include "Audio/DominationSystemAnnouncer.h"

ADominationSystemAnnouncer::ADominationSystemAnnouncer()
{
	FSystemAnnouncerCue ActivatedCue;
	ActivatedCue.Priority = 10;
	Announcements.Add(DominationAnnouncementIds::ObjectiveActivated, ActivatedCue);

	FSystemAnnouncerCue StartedCue;
	StartedCue.Priority = 20;
	Announcements.Add(DominationAnnouncementIds::CaptureStarted, StartedCue);

	FSystemAnnouncerCue CompletedCue;
	CompletedCue.Priority = 100;
	CompletedCue.bInterruptLowerPriority = true;
	Announcements.Add(DominationAnnouncementIds::CaptureCompleted, CompletedCue);
}
