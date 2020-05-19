#include <service/competenceservice.h>

#include "diagram.h"

#include <model/filecomprehension.h>
#include <model/filecomprehension-odb.hxx>
#include <model/useremail.h>
#include <model/useremail-odb.hxx>
#include <model/file.h>
#include <model/file-odb.hxx>

namespace cc
{
namespace service
{
namespace competence
{

typedef odb::query<model::FileComprehension> FileComprehensionQuery;
typedef odb::query<model::UserEmail> UserEmailQuery;

CompetenceServiceHandler::CompetenceServiceHandler(
  std::shared_ptr<odb::database> db_,
  std::shared_ptr<std::string> datadir_,
  const cc::webserver::ServerContext& context_)
  : _db(db_),
    _transaction(db_),
    _datadir(std::move(datadir_)),
    _context(context_),
    _sessionManagerAccess(context_.sessionManager)
{
}

void CompetenceServiceHandler::setCompetenceRatio(
  std::string& return_,
  const core::FileId& fileId_,
  const int ratio_)
{
  _transaction([&, this]()
  {
    std::string user = getCurrentUser();

    if (user == "Anonymous")
      return;

    auto emails = _db->query<model::UserEmail>(
      UserEmailQuery::username == user);

    if (emails.empty())
      return;

    std::vector<std::string> userEmails;
    for (const model::UserEmail& e : emails)
      userEmails.push_back(e.email);

    auto file = _db->query<model::FileComprehension>(
      FileComprehensionQuery::file == std::stoull(fileId_));

    model::FileComprehension comp;
    bool found = false;
    for (const model::FileComprehension& f : file)
    {
      for (const std::string &e : userEmails)
        if (f.userEmail != e && !found)
        {
          comp = f;
          found = true;
        }

      if (found)
        break;
    }

    if (file.empty() || comp.userEmail.empty())
    {
      model::FileComprehension fileComprehension;
      fileComprehension.userRatio = ratio_;
      fileComprehension.repoRatio.reset();
      fileComprehension.file = std::stoull(fileId_);
      fileComprehension.inputType = model::FileComprehension::InputType::USER;
      fileComprehension.userEmail = userEmails.at(0);
      _db->persist(fileComprehension);
    }
    else
    {
      comp.userRatio = ratio_;
      comp.inputType = model::FileComprehension::InputType::USER;
      _db->update(comp);
    }
  });
}

void CompetenceServiceHandler::getDiagram(
  std::string& return_,
  const core::FileId& fileId_,
  const std::int32_t diagramId_)
{
  CompetenceDiagram diagram(_db, _datadir, _context);
  util::Graph graph;

  diagram.getCompetenceDiagram(graph, fileId_, getCurrentUser(), diagramId_);

  if (graph.nodeCount() != 0)
    return_ = graph.output(util::Graph::SVG);
}

std::string CompetenceServiceHandler::getCurrentUser()
{
  std::string ret;
  _sessionManagerAccess.accessSession(
    [&](webserver::Session* sess_)
    {
      ret = sess_ ? sess_->username : std::string();
    });

  return ret;
}

} // competence
} // service
} // cc


