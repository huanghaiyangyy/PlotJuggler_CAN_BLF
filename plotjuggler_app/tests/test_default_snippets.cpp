#include <gtest/gtest.h>

#include <QDomDocument>
#include <QFile>
#include <QString>

namespace
{

QString defaultSnippetsPath()
{
  return QString(PJ_PROJECT_SOURCE_DIR) + "/plotjuggler_app/resources/default.snippets.xml";
}

QDomElement findSnippetByName(const QDomDocument& doc, const QString& name)
{
  const auto root = doc.documentElement();
  for (auto elem = root.firstChildElement("snippet"); !elem.isNull();
       elem = elem.nextSiblingElement("snippet"))
  {
    if (elem.attribute("name") == name)
    {
      return elem;
    }
  }
  return {};
}

QDomDocument loadDefaultSnippets()
{
  const auto path = defaultSnippetsPath();
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << "Failed to open: " << path.toStdString();

  QDomDocument doc;
  QString parse_error;
  int parse_error_line = -1;
  EXPECT_TRUE(doc.setContent(&file, &parse_error, &parse_error_line))
      << "Failed to parse default snippets xml at line " << parse_error_line << ": "
      << parse_error.toStdString();

  return doc;
}

}  // namespace

TEST(DefaultSnippets, IncludesMovingAverageAndLowPassFilters)
{
  const auto doc = loadDefaultSnippets();

  EXPECT_FALSE(findSnippetByName(doc, "01_ax_mv_avg_flt").isNull());
  EXPECT_FALSE(findSnippetByName(doc, "02_delta_v_from_speed").isNull());
  EXPECT_FALSE(findSnippetByName(doc, "03_dvx_mv_avg").isNull());
  EXPECT_FALSE(findSnippetByName(doc, "04_dvx_low_pass").isNull());
}

TEST(DefaultSnippets, NewSnippetsContainExpectedConfigVariables)
{
  const auto doc = loadDefaultSnippets();

  const auto ax_mv_avg = findSnippetByName(doc, "01_ax_mv_avg_flt");
  ASSERT_FALSE(ax_mv_avg.isNull());
  const auto ax_mv_avg_global = ax_mv_avg.firstChildElement("global").text();
  const auto ax_mv_avg_function = ax_mv_avg.firstChildElement("function").text();
  EXPECT_TRUE(ax_mv_avg_global.contains("ax_mv_avg_filter_window_size"));
  EXPECT_TRUE(ax_mv_avg_global.contains("ax_mv_avg_flt_offset"));
  EXPECT_TRUE(ax_mv_avg_global.contains("ax_mv_avg_flt_time_buffer"));
  EXPECT_TRUE(ax_mv_avg_global.contains("ax_mv_avg_flt_compensate_time"));
  EXPECT_TRUE(ax_mv_avg_function.contains("ax_mv_avg_filter_window_size"));
  EXPECT_TRUE(ax_mv_avg_function.contains("return out_time"));

  const auto delta_v = findSnippetByName(doc, "02_delta_v_from_speed");
  ASSERT_FALSE(delta_v.isNull());
  const auto delta_v_global = delta_v.firstChildElement("global").text();
  const auto delta_v_function = delta_v.firstChildElement("function").text();
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_v_ma_window_size"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_v_ma_buffer"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_v_ma_sum"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_min"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_max"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_prev_speed_ma"));
  EXPECT_TRUE(delta_v_global.contains("delta_v_from_speed_prev_time"));
  EXPECT_TRUE(delta_v_function.contains("speed_ma"));
  EXPECT_TRUE(delta_v_function.contains("delta_v_from_speed_v_ma_window_size"));
  EXPECT_TRUE(delta_v_function.contains("delta_v = delta_v_from_speed_min"));
  EXPECT_TRUE(delta_v_function.contains("delta_v = delta_v_from_speed_max"));
  EXPECT_TRUE(delta_v_function.contains("/ 3.6"));
  EXPECT_TRUE(delta_v_function.contains("dt > 0"));

  const auto dvx_mv_avg = findSnippetByName(doc, "03_dvx_mv_avg");
  ASSERT_FALSE(dvx_mv_avg.isNull());
  const auto dvx_mv_avg_global = dvx_mv_avg.firstChildElement("global").text();
  const auto dvx_mv_avg_function = dvx_mv_avg.firstChildElement("function").text();
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_v_ma_window_size"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_v_ma_buffer"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_v_ma_time_buffer"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_v_ma_sum"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_prev_speed_ma"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_prev_time"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_time_buffer"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_compensate_time"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_filter_window_size"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_min"));
  EXPECT_TRUE(dvx_mv_avg_global.contains("dvx_mv_avg_max"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("speed_ma"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("speed_ma_time"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("delta_v_time"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("/ 3.6"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("delta_v = dvx_mv_avg_min"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("delta_v = dvx_mv_avg_max"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("dvx_mv_avg_buffer"));
  EXPECT_TRUE(dvx_mv_avg_function.contains("return out_time"));

  const auto dvx_low_pass = findSnippetByName(doc, "04_dvx_low_pass");
  ASSERT_FALSE(dvx_low_pass.isNull());
  const auto dvx_low_pass_global = dvx_low_pass.firstChildElement("global").text();
  const auto dvx_low_pass_function = dvx_low_pass.firstChildElement("function").text();
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_filter_alpha"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_v_ma_window_size"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_v_ma_buffer"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_v_ma_sum"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_prev_speed_ma"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_prev_time"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_min"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_max"));
  EXPECT_TRUE(dvx_low_pass_global.contains("dvx_low_pass_initialized_acc"));
  EXPECT_TRUE(dvx_low_pass_function.contains("dvx_low_pass_prev"));
  EXPECT_TRUE(dvx_low_pass_function.contains("speed_ma"));
  EXPECT_TRUE(dvx_low_pass_function.contains("/ 3.6"));
  EXPECT_TRUE(dvx_low_pass_function.contains("delta_v = dvx_low_pass_min"));
  EXPECT_TRUE(dvx_low_pass_function.contains("delta_v = dvx_low_pass_max"));
}
