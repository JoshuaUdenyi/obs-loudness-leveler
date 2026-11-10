/* A dock panel (View > Docks > Loudness Leveler) with visual loudness bars.
 * Drag it next to the Audio Mixer and leave it there. */

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include "meter-api.h"
#include "text.h"

namespace {

const char *kDockId = "loudness_leveler_meter";
bool g_dock_added = false;

constexpr double kMinLufs = -60.0; /* left edge of every bar */
constexpr int kRowHeight = 74;
constexpr int kPad = 8;
constexpr int kBarHeight = 12;
constexpr int kLabelWidth = 30;
constexpr int kValueWidth = 44;

class MeterWidget : public QWidget {
public:
	explicit MeterWidget(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMinimumWidth(200);
		setMinimumHeight(kRowHeight);

		QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
			count_ = ll_meter_snapshot(rows_, LL_MAX_METERS);
			int wanted = std::max(1, count_) * kRowHeight + kPad;
			if (minimumHeight() != wanted)
				setMinimumHeight(wanted);
			update();
		});
		timer_.start(50); /* 20 updates per second */
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);

		const QPalette &pal = palette();
		const QColor text = pal.color(QPalette::WindowText);
		const QColor track = pal.color(QPalette::Base);
		const QColor outline = pal.color(QPalette::Mid);

		if (count_ == 0) {
			p.setPen(text);
			p.drawText(rect().adjusted(kPad, kPad, -kPad, -kPad),
				   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				   QString::fromUtf8(ll_text("Dock.Empty")));
			return;
		}

		for (int i = 0; i < count_; i++)
			drawRow(p, rows_[i], i * kRowHeight + kPad / 2, text, track, outline);
	}

private:
	void drawRow(QPainter &p, const ll_meter_row &r, int y, const QColor &text, const QColor &track,
		     const QColor &outline)
	{
		p.setOpacity(r.bypass ? 0.45 : 1.0);

		/* --- title line: source name on the left, status on the right --- */
		QFont bold = p.font();
		bold.setBold(true);
		p.setFont(bold);
		p.setPen(text);
		QFontMetrics fm(bold);
		QString name = fm.elidedText(QString::fromUtf8(r.source), Qt::ElideRight, width() / 2);
		p.drawText(QRect(kPad, y, width() - 2 * kPad, 18), Qt::AlignLeft | Qt::AlignVCenter, name);

		QFont normal = bold;
		normal.setBold(false);
		p.setFont(normal);

		QString status;
		if (r.bypass) {
			status = QString::fromUtf8(ll_text("Dock.Bypassed"));
		} else {
			status = QString::fromUtf8(ll_text("Dock.Gain")) + QString::asprintf(" %+.1f dB", r.gain_db);
			if (r.limiter_db < -0.1)
				status += QString::fromUtf8("   ") + QString::fromUtf8(ll_text("Dock.Limiter")) +
					  QString::asprintf(" %.1f dB", r.limiter_db);
		}
		p.drawText(QRect(kPad, y, width() - 2 * kPad, 18), Qt::AlignRight | Qt::AlignVCenter, status);

		/* --- bars --- */
		const QColor inputColor(70, 140, 220);
		drawBar(p, y + 24, ll_text("Dock.In"), r.in_lufs, inputColor, false, 0.0, text, track, outline);

		QColor outColor(60, 190, 100); /* green: on target */
		if (!r.agc || r.bypass) {
			outColor = inputColor;
		} else {
			double off = std::abs(r.out_lufs - r.target_lufs);
			if (off > 5.0)
				outColor = QColor(220, 80, 70);
			else if (off > 2.0)
				outColor = QColor(225, 165, 50);
		}
		drawBar(p, y + 44, ll_text("Dock.Out"), r.out_lufs, outColor, r.agc && !r.bypass, r.target_lufs, text,
			track, outline);

		p.setOpacity(1.0);
	}

	void drawBar(QPainter &p, int y, const char *label, double lufs, const QColor &fill, bool showTarget,
		     double target, const QColor &text, const QColor &track, const QColor &outline)
	{
		const int barX = kPad + kLabelWidth;
		const int barW = std::max(20, width() - barX - kValueWidth - kPad);

		auto xFor = [&](double v) {
			double f = std::clamp((v - kMinLufs) / -kMinLufs, 0.0, 1.0);
			return barX + static_cast<int>(f * barW);
		};

		/* label */
		p.setPen(text);
		p.drawText(QRect(kPad, y - 2, kLabelWidth, kBarHeight + 4), Qt::AlignLeft | Qt::AlignVCenter,
			   QString::fromUtf8(label));

		/* track */
		p.setPen(QPen(outline, 1));
		p.setBrush(track);
		p.drawRoundedRect(QRect(barX, y, barW, kBarHeight), 2, 2);

		/* level */
		const bool silent = lufs <= kMinLufs + 0.1;
		if (!silent) {
			p.setPen(Qt::NoPen);
			p.setBrush(fill);
			p.drawRoundedRect(QRect(barX + 1, y + 1, std::max(1, xFor(lufs) - barX - 1), kBarHeight - 2), 2,
					  2);
		}

		/* target marker */
		if (showTarget) {
			p.setPen(QPen(text, 2));
			int tx = xFor(target);
			p.drawLine(tx, y - 3, tx, y + kBarHeight + 3);
		}

		/* value */
		p.setPen(text);
		QString value = silent ? QString::fromUtf8("--") : QString::number(lufs, 'f', 1);
		p.drawText(QRect(barX + barW + 4, y - 2, kValueWidth, kBarHeight + 4), Qt::AlignLeft | Qt::AlignVCenter,
			   value);
	}

	QTimer timer_;
	ll_meter_row rows_[LL_MAX_METERS];
	int count_ = 0;
};

void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !g_dock_added) {
		auto *widget = new MeterWidget();
		widget->setObjectName("LoudnessLevelerMeter");
		/* On success OBS owns the widget and puts it in a dock. */
		g_dock_added = obs_frontend_add_dock_by_id(kDockId, ll_text("Dock.Title"), widget);
		if (!g_dock_added)
			delete widget;
	} else if (event == OBS_FRONTEND_EVENT_EXIT && g_dock_added) {
		obs_frontend_remove_dock(kDockId);
		g_dock_added = false;
	}
}

} // namespace

extern "C" void ll_dock_init(void)
{
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
}

extern "C" void ll_dock_free(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
}
