#include "TestQtCallback.h"

#include "SafeBinder.h"

#include <QtCore/QDebug>
#include <QtCore/QEventLoop>

#if QT_VERSION >= QT_VERSION_CHECK(4,8,0)
#include <QtCore/QElapsedTimer>
#endif

#include <iostream>

#if defined(QST_USE_CPP11_LIBS)
using namespace std;
using namespace std::placeholders;
#else
using namespace std::tr1;
using namespace std::tr1::placeholders;
#endif

using namespace QtSignalTools;

void TestQtCallback::testInvoke()
{
	CallbackTester tester;
	QtCallback callback1(&tester, SLOT(addValue(int)));
	callback1.bind(42);
	callback1.invoke();
	QCOMPARE(tester.values, QList<int>() << 42);
	tester.values.clear();

	QtCallback1<int> callback2(&tester, SLOT(addValue(int)));
	callback2.invoke(27);
	QCOMPARE(tester.values, QList<int>() << 27);
}

void TestQtCallback::testSignalProxy()
{
	CallbackTester tester;
	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)),
	  QtCallback(&tester, SLOT(addValue(int))));
	tester.emitASignal(32);

	QCOMPARE(tester.values, QList<int>() << 32);
	tester.values.clear();

	QtSignalForwarder::disconnect(&tester, SIGNAL(aSignal(int)));

	tester.emitASignal(15);
	QCOMPARE(tester.values, QList<int>());
	tester.values.clear();

	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)),
	  QtCallback(&tester, SLOT(addValue(int))).bind(10));
	tester.emitASignal(11);
	QCOMPARE(tester.values, QList<int>() << 10);
	tester.values.clear();

	QtSignalForwarder::connect(&tester, SIGNAL(noArgSignal()),
	  QtCallback(&tester, SLOT(addValue(int))));
	tester.emitNoArgSignal();
	QCOMPARE(tester.values, QList<int>());
}

void TestQtCallback::testEventProxy()
{
	CallbackTester tester;
	QtSignalForwarder::connect(&tester, QEvent::MouseButtonPress,
	  QtCallback(&tester, SLOT(addValue(int))).bind(1));
	QMouseEvent event(QEvent::MouseButtonPress, QPoint(0,0), Qt::LeftButton, Qt::LeftButton, 0);
	QCoreApplication::sendEvent(&tester, &event);
	QCOMPARE(tester.values, QList<int>() << 1);
	tester.values.clear();

	QtSignalForwarder::disconnect(&tester, QEvent::MouseButtonPress);
	QCoreApplication::sendEvent(&tester, &event);
	QCOMPARE(tester.values, QList<int>());
}

void TestQtCallback::testSignalToFunctionObject()
{
	CallbackTester tester;
	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)),
	  function<void()>(bind(&CallbackTester::addValue, &tester, 18)));
	tester.emitASignal(42);
	QCOMPARE(tester.values, QList<int>() << 18);
	tester.values.clear();

	QtSignalForwarder::disconnect(&tester, SIGNAL(aSignal(int)));
	tester.emitASignal(19);
	QCOMPARE(tester.values, QList<int>());
	tester.values.clear();

	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)),
	  function<void(int)>(bind(&CallbackTester::addValue, &tester, _1)));
	tester.emitASignal(39);
	QCOMPARE(tester.values, QList<int>() << 39);

	// check that a signal arg count mismatch
	// is caught
	tester.values.clear();
	QtSignalForwarder::connect(&tester, SIGNAL(noArgSignal()),
	  function<void(int)>(bind(&CallbackTester::addValue, &tester, _1)));
	tester.emitNoArgSignal();
	QCOMPARE(tester.values, QList<int>());
}

int sumInputs(int value)
{
	static int total = 0;
	total += value;
	return total;
}

void TestQtCallback::testSignalToPlainFunc()
{
	CallbackTester tester;
	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), sumInputs);
	tester.emitASignal(5);
	tester.emitASignal(8);
	QCOMPARE(sumInputs(0), 13);
}

void TestQtCallback::testArgCast()
{
	// use function and bind() to perform a cast of the argument
	// when it is emitted.
	QList<qint64> list;
	CallbackTester tester;
	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)),
	  function<void(int)>(bind(&QList<qint64>::push_back, &list, _1)));
	tester.emitASignal(42);
	QCOMPARE(list, QList<qint64>() << 42LL);
}

void floatFunc(float) {}
void intFunc(int) {}
void noArgsFunc() {}
void twoArgsFunc(int,int) {}

void TestQtCallback::testArgTypeCheck()
{
	CallbackTester tester;
	QVERIFY(QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), intFunc));
	QVERIFY(!QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), floatFunc));
	QVERIFY(QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), noArgsFunc));
	QVERIFY(!QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), twoArgsFunc));
}

void fiveArgFunc(int,bool,float,char,double) {}

void TestQtCallback::testArgLimit()
{
	QtMetacallAdapter adapter(fiveArgFunc);
	QtMetacallArgsArray args = {-1};
	int count = adapter.getArgTypes(args);
	QStringList argList;
	for (int i=0; i < count; i++) {
		argList << QMetaType::typeName(args[i]);
	}
	QCOMPARE(argList, QStringList() << "int" << "bool" << "float" << "char" << "double");
}

void TestQtCallback::testSignalToLambda()
{
#ifdef QST_COMPILER_SUPPORTS_LAMBDAS
	CallbackTester tester;
	int sum = 0;
	QtSignalForwarder::connect(&tester, SIGNAL(aSignal(int)), function<void(int)>(
										 [&](int value) { sum += value; }
	));
	tester.emitASignal(12);
	tester.emitASignal(7);
	QCOMPARE(sum, 19);
#else
	QSKIP("Compiler does not support C++11 lambdas", SkipAll);
#endif
}

void TestQtCallback::testSenderDestroyed()
{
	QScopedPointer<CallbackTester> tester(new CallbackTester);
	QtSignalForwarder proxy;
	proxy.bind(tester.data(), SIGNAL(aSignal(int)),
	  noArgsFunc);
	QCOMPARE(proxy.bindingCount(), 1);
	tester.reset();
	QCOMPARE(proxy.bindingCount(), 0);

	tester.reset(new CallbackTester);
	proxy.bind(tester.data(), QEvent::MouseButtonPress, noArgsFunc);
	QCOMPARE(proxy.bindingCount(), 1);
	tester.reset();
	QCOMPARE(proxy.bindingCount(), 0);
}

void TestQtCallback::testUnbind()
{
	CallbackTester tester;
	QtSignalForwarder proxy;
	
	proxy.bind(&tester, SIGNAL(aSignal(int)), noArgsFunc);
	QCOMPARE(proxy.bindingCount(), 1);
	QVERIFY(proxy.isConnected(&tester));

	QCOMPARE(tester.receiverCount(SIGNAL(aSignal(int))), 1);
	QCOMPARE(tester.receiverCount(SIGNAL(destroyed(QObject*))), 1);

	proxy.bind(&tester, QEvent::MouseButtonPress, noArgsFunc);
	QCOMPARE(proxy.bindingCount(), 2);

	proxy.unbind(&tester, SIGNAL(aSignal(int)));
	QCOMPARE(proxy.bindingCount(), 1);
	QVERIFY(proxy.isConnected(&tester));

	// check that the proxy is still listening for destruction
	// of the sender
	QCOMPARE(tester.receiverCount(SIGNAL(destroyed(QObject*))), 1);

	proxy.unbind(&tester, QEvent::MouseButtonPress);
	QCOMPARE(proxy.bindingCount(), 0);
	QVERIFY(!proxy.isConnected(&tester));

	// check that the proxy is no longer listening for destruction
	// of the sender
	QCOMPARE(tester.receiverCount(SIGNAL(destroyed(QObject*))), 0);
}

void TestQtCallback::testConnectPerf()
{
#if QT_VERSION >= QT_VERSION_CHECK(4,8,0)
	QSKIP("Benchmark disabled", SkipAll);

	CallbackTester receiver;

	int objCount = 2;

	for (int i = 0; i < 15; i++) {
		QElapsedTimer timer;
		timer.start();

		QVector<CallbackTester*> objectList;
		for (int k=0; k < objCount; k++) {
			objectList << new CallbackTester;
			QtSignalForwarder::connect(objectList.last(), SIGNAL(aSignal(int)),
					function<void(int)>(bind(&CallbackTester::addValue, &receiver, 42)));
			objectList.last()->emitASignal(32);
		}
		qDeleteAll(objectList);
		objectList.clear();

		qreal totalMs = timer.nsecsElapsed() / (1000 * 1000);
		qreal meanMs = totalMs/objCount;
		qDebug() << "cost per obj for" << objCount << "objects" << meanMs << "total" << totalMs;

		objCount *= 2;
	}
#endif
}

void TestQtCallback::testDelayedCall()
{
#if QT_VERSION >= QT_VERSION_CHECK(4,8,0)
	CallbackTester tester;
	QEventLoop loop;

	connect(&tester, SIGNAL(valuesChanged()), &loop, SLOT(quit()));
	
	// on Windows under Qt 4, the timeout occasionally expires
	// slightly before the specified interval, so here we
	// only check that there was some delay before
	// the timeout occurred.
	const int CALL_DELAY = 50;
	const int MIN_ACTUAL_DELAY = 25;

	QElapsedTimer timer;
	timer.start();
	QtSignalForwarder::delayedCall(CALL_DELAY, function<void()>(bind(&CallbackTester::addValue, &tester, 42)));
	loop.exec();

	QVERIFY(timer.elapsed() >= MIN_ACTUAL_DELAY);
	QCOMPARE(tester.values, QList<int>() << 42);
#endif
}

void TestQtCallback::testSafeBinder()
{
	// test with a QObject
	QObject* object = new QObject;
	object->setObjectName("testObject");
	function<QString()> getName(safe_bind(object, &QObject::objectName));
	QCOMPARE(getName(), QString("testObject"));
	delete object;
	object = 0;
	QCOMPARE(getName(), QString());

	CallbackTester tester;
	object = new QObject;

	// test with QObject + QtSignalForwarder
	QtSignalForwarder::connect(&tester, SIGNAL(stringSignal(QString)),
	  function<void(QString)>(safe_bind(object, &QObject::setObjectName)));
	tester.emitStringSignal("testObject2");
	QCOMPARE(object->objectName(), QString("testObject2"));
	delete object;

	// try changing the name, this will have no effect now
	// as the object has been destroyed
	tester.emitStringSignal("newName");

	// test with something that is not
	// a QObject
	shared_ptr<QString> string(new QString);
	string->append("testString");
	function<QString()> getTrimmed(safe_bind(weak_ptr<QString>(string), &QString::trimmed));
	QCOMPARE(getTrimmed(), QString("testString"));
	string.reset();
	QCOMPARE(getTrimmed(), QString());
}

struct CallCounter
{
	CallCounter()
		: count(0)
	{}

	int count;

	void increment() {
		++count;
	}
};

function<void()> incrementFunc(CallCounter& counter)
{
	return bind(&CallCounter::increment, &counter);
}

void TestQtCallback::testBindingCount()
{
	CallCounter firstSignalCall;
	CallCounter secondSignalCall;

	CallCounter firstEventCall;
	CallCounter secondEventCall;

	CallbackTester tester;

	QtSignalForwarder forwarder;
	forwarder.bind(&tester, SIGNAL(aSignal(int)), incrementFunc(firstSignalCall));
	forwarder.bind(&tester, SIGNAL(aSignal(int)), incrementFunc(secondSignalCall));
	QCOMPARE(forwarder.bindingCount(), 2);

	tester.emitASignal(1);
	QCOMPARE(firstSignalCall.count, 1);
	QCOMPARE(secondSignalCall.count, 1);

	forwarder.unbind(&tester);
	QCOMPARE(forwarder.bindingCount(), 0);

	forwarder.bind(&tester, QEvent::Enter, incrementFunc(firstEventCall));
	forwarder.bind(&tester, QEvent::Leave, incrementFunc(secondEventCall));
	QCOMPARE(forwarder.bindingCount(), 2);

	QEvent enterEvent(QEvent::Enter);
	QEvent leaveEvent(QEvent::Leave);
	QCoreApplication::sendEvent(&tester, &enterEvent);
	QCoreApplication::sendEvent(&tester, &leaveEvent);

	QCOMPARE(firstEventCall.count, 1);
	QCOMPARE(secondEventCall.count, 1);

	forwarder.unbind(&tester);
	QCOMPARE(forwarder.bindingCount(), 0);
}

QTEST_MAIN(TestQtCallback)
