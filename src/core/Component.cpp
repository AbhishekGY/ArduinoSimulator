#include "core/Component.h"

Component::Component(const QString &name, QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid())
    , m_name(name)
    , m_circuit(nullptr)
{
}
