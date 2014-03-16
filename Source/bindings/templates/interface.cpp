{% extends 'interface_base.cpp' %}


{##############################################################################}
{% macro attribute_configuration(attribute) %}
{% set getter_callback =
       '%sV8Internal::%sAttributeGetterCallback' %
            (cpp_class, attribute.name)
       if not attribute.constructor_type else
       ('%sV8Internal::%sConstructorGetterCallback' %
            (cpp_class, attribute.name)
        if attribute.needs_constructor_getter_callback else
       '{0}V8Internal::{0}ConstructorGetter'.format(cpp_class)) %}
{% set getter_callback_for_main_world =
       '%sV8Internal::%sAttributeGetterCallbackForMainWorld' %
            (cpp_class, attribute.name)
       if attribute.is_per_world_bindings else '0' %}
{% set setter_callback = attribute.setter_callback %}
{% set setter_callback_for_main_world =
       '%sV8Internal::%sAttributeSetterCallbackForMainWorld' %
           (cpp_class, attribute.name)
       if attribute.is_per_world_bindings and
          (not attribute.is_read_only or attribute.put_forwards) else '0' %}
{% set wrapper_type_info =
       'const_cast<WrapperTypeInfo*>(&V8%s::wrapperTypeInfo)' %
            attribute.constructor_type
        if attribute.constructor_type else '0' %}
{% set access_control = 'static_cast<v8::AccessControl>(%s)' %
                        ' | '.join(attribute.access_control_list) %}
{% set property_attribute = 'static_cast<v8::PropertyAttribute>(%s)' %
                            ' | '.join(attribute.property_attributes) %}
{% set on_prototype = '1 /* on prototype */'
       if interface_name == 'Window' and attribute.idl_type == 'EventHandler'
       else '0 /* on instance */' %}
{% set attribute_configuration_list = [
       '"%s"' % attribute.name,
       getter_callback,
       setter_callback,
       getter_callback_for_main_world,
       setter_callback_for_main_world,
       wrapper_type_info,
       access_control,
       property_attribute,
   ] %}
{% if not attribute.is_expose_js_accessors %}
{% set attribute_configuration_list = attribute_configuration_list
                                    + [on_prototype] %}
{% endif %}
{{'{'}}{{attribute_configuration_list|join(', ')}}{{'}'}}
{%- endmacro %}


{##############################################################################}
{% macro method_configuration(method) %}
{% set method_callback =
   '%sV8Internal::%sMethodCallback' % (cpp_class, method.name) %}
{% set method_callback_for_main_world =
   '%sV8Internal::%sMethodCallbackForMainWorld' % (cpp_class, method.name)
   if method.is_per_world_bindings else '0' %}
{"{{method.name}}", {{method_callback}}, {{method_callback_for_main_world}}, {{method.number_of_required_or_variadic_arguments}}}
{%- endmacro %}


{##############################################################################}
{% block constructor_getter %}
{% if has_constructor_attributes %}
static void {{cpp_class}}ConstructorGetter(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    v8::Handle<v8::Value> data = info.Data();
    ASSERT(data->IsExternal());
    V8PerContextData* perContextData = V8PerContextData::from(info.Holder()->CreationContext());
    if (!perContextData)
        return;
    v8SetReturnValue(info, perContextData->constructorForType(WrapperTypeInfo::unwrap(data)));
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block replaceable_attribute_setter_and_callback %}
{% if has_replaceable_attributes or has_constructor_attributes %}
{# FIXME: rename to ForceSetAttributeOnThis, since also used for Constructors #}
static void {{cpp_class}}ReplaceableAttributeSetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    {% if is_check_security %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    info.This()->ForceSet(name, jsValue);
}

{# FIXME: rename to ForceSetAttributeOnThisCallback, since also used for Constructors #}
static void {{cpp_class}}ReplaceableAttributeSetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    {{cpp_class}}V8Internal::{{cpp_class}}ReplaceableAttributeSetter(name, jsValue, info);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block security_check_functions %}
{% if is_check_security and interface_name != 'Window' %}
bool indexedSecurityCheck(v8::Local<v8::Object> host, uint32_t index, v8::AccessType type, v8::Local<v8::Value>)
{
    {{cpp_class}}* imp =  {{v8_class}}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(v8::Isolate::GetCurrent(), imp->frame(), DoNotReportSecurityError);
}

bool namedSecurityCheck(v8::Local<v8::Object> host, v8::Local<v8::Value> key, v8::AccessType type, v8::Local<v8::Value>)
{
    {{cpp_class}}* imp =  {{v8_class}}::toNative(host);
    return BindingSecurity::shouldAllowAccessToFrame(v8::Isolate::GetCurrent(), imp->frame(), DoNotReportSecurityError);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_getter %}
{% if indexed_property_getter and not indexed_property_getter.is_custom %}
{% set getter = indexed_property_getter %}
static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% if getter.is_raises_exception %}
    ExceptionState exceptionState(ExceptionState::IndexedGetterContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% set getter_name = getter.name or 'anonymousIndexedGetter' %}
    {% set getter_arguments = ['index', 'exceptionState']
           if getter.is_raises_exception else ['index'] %}
    {{getter.cpp_type}} result = imp->{{getter_name}}({{getter_arguments|join(', ')}});
    {% if getter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if ({{getter.is_null_expression}})
        return;
    {{getter.v8_set_return_value}};
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_getter_callback %}
{% if indexed_property_getter %}
{% set getter = indexed_property_getter %}
static void indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    {% if getter.is_custom %}
    {{v8_class}}::indexedPropertyGetterCustom(index, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertyGetter(index, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_setter %}
{% if indexed_property_setter and not indexed_property_setter.is_custom %}
{% set setter = indexed_property_setter %}
static void indexedPropertySetter(uint32_t index, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {{setter.v8_value_to_local_cpp_value}};
    {% if setter.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::IndexedSetterContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if setter.has_strict_type_checking %}
    {# Type checking for interface types (if interface not implemented, throw
       TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
    if (!isUndefinedOrNull(jsValue) && !V8{{setter.idl_type}}::hasInstance(jsValue, info.GetIsolate())) {
        exceptionState.throwTypeError("The provided value is not of type '{{setter.idl_type}}'.");
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% set setter_name = setter.name or 'anonymousIndexedSetter' %}
    {% set setter_arguments = ['index', 'propertyValue', 'exceptionState']
           if setter.is_raises_exception else ['index', 'propertyValue'] %}
    bool result = imp->{{setter_name}}({{setter_arguments|join(', ')}});
    {% if setter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (!result)
        return;
    v8SetReturnValue(info, jsValue);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_setter_callback %}
{% if indexed_property_setter %}
{% set setter = indexed_property_setter %}
static void indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    {% if setter.is_custom %}
    {{v8_class}}::indexedPropertySetterCustom(index, jsValue, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertySetter(index, jsValue, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_deleter %}
{% if indexed_property_deleter and not indexed_property_deleter.is_custom %}
{% set deleter = indexed_property_deleter %}
static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% if deleter.is_raises_exception %}
    ExceptionState exceptionState(ExceptionState::IndexedDeletionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% set deleter_name = deleter.name or 'anonymousIndexedDeleter' %}
    {% set deleter_arguments = ['index', 'exceptionState']
           if deleter.is_raises_exception else ['index'] %}
    DeleteResult result = imp->{{deleter_name}}({{deleter_arguments|join(', ')}});
    {% if deleter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (result != DeleteUnknownProperty)
        return v8SetReturnValueBool(info, result == DeleteSuccess);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block indexed_property_deleter_callback %}
{% if indexed_property_deleter %}
{% set deleter = indexed_property_deleter %}
static void indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    {% if deleter.is_custom %}
    {{v8_class}}::indexedPropertyDeleterCustom(index, info);
    {% else %}
    {{cpp_class}}V8Internal::indexedPropertyDeleter(index, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% from 'methods.cpp' import union_type_method_call %}
{% block named_property_getter %}
{% if named_property_getter and not named_property_getter.is_custom %}
{% set getter = named_property_getter %}
static void namedPropertyGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% if not is_override_builtins %}
    if (info.Holder()->HasRealNamedProperty(name))
        return;
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return;

    {% endif %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name);
    {% if getter.is_raises_exception %}
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::GetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if getter.union_arguments %}
    {{union_type_method_call(getter) | indent}}
    {% else %}
    {{getter.cpp_type}} result = {{getter.cpp_value}};
    {% if getter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if ({{getter.is_null_expression}})
        return;
    {{getter.v8_set_return_value}};
    {% endif %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_getter_callback %}
{% if named_property_getter %}
{% set getter = named_property_getter %}
static void namedPropertyGetterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    {% if getter.is_custom %}
    {{v8_class}}::namedPropertyGetterCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyGetter(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_setter %}
{% if named_property_setter and not named_property_setter.is_custom %}
{% set setter = named_property_setter %}
static void namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% if not is_override_builtins %}
    if (info.Holder()->HasRealNamedProperty(name))
        return;
    if (!info.Holder()->GetRealNamedPropertyInPrototypeChain(name).IsEmpty())
        return;

    {% endif %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {# v8_value_to_local_cpp_value('DOMString', 'name', 'propertyName') #}
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, propertyName, name);
    {{setter.v8_value_to_local_cpp_value}};
    {% if setter.has_exception_state %}
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% set setter_name = setter.name or 'anonymousNamedSetter' %}
    {% set setter_arguments =
           ['propertyName', 'propertyValue', 'exceptionState']
           if setter.is_raises_exception else
           ['propertyName', 'propertyValue'] %}
    bool result = imp->{{setter_name}}({{setter_arguments|join(', ')}});
    {% if setter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (!result)
        return;
    v8SetReturnValue(info, jsValue);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_setter_callback %}
{% if named_property_setter %}
{% set setter = named_property_setter %}
static void namedPropertySetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    {% if setter.is_custom %}
    {{v8_class}}::namedPropertySetterCustom(name, jsValue, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertySetter(name, jsValue, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_query %}
{% if named_property_getter and named_property_getter.is_enumerable and
      not named_property_getter.is_custom_property_query %}
{# If there is an enumerator, there MUST be a query method to properly
   communicate property attributes. #}
static void namedPropertyQuery(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name);
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::GetterContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    bool result = imp->namedPropertyQuery(propertyName, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    if (!result)
        return;
    v8SetReturnValueInt(info, v8::None);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_query_callback %}
{% if named_property_getter and named_property_getter.is_enumerable %}
{% set getter = named_property_getter %}
static void namedPropertyQueryCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    {% if getter.is_custom_property_query %}
    {{v8_class}}::namedPropertyQueryCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyQuery(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_deleter %}
{% if named_property_deleter and not named_property_deleter.is_custom %}
{% set deleter = named_property_deleter %}
static void namedPropertyDeleter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    AtomicString propertyName = toCoreAtomicString(name);
    {% if deleter.is_raises_exception %}
    v8::String::Utf8Value namedProperty(name);
    ExceptionState exceptionState(ExceptionState::DeletionContext, *namedProperty, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% set deleter_name = deleter.name or 'anonymousNamedDeleter' %}
    {% set deleter_arguments = ['propertyName', 'exceptionState']
           if deleter.is_raises_exception else ['propertyName'] %}
    DeleteResult result = imp->{{deleter_name}}({{deleter_arguments|join(', ')}});
    {% if deleter.is_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}
    if (result != DeleteUnknownProperty)
        return v8SetReturnValueBool(info, result == DeleteSuccess);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_deleter_callback %}
{% if named_property_deleter %}
{% set deleter = named_property_deleter %}
static void namedPropertyDeleterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    {% if deleter.is_custom %}
    {{v8_class}}::namedPropertyDeleterCustom(name, info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyDeleter(name, info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_enumerator %}
{% if named_property_getter and named_property_getter.is_enumerable and
      not named_property_getter.is_custom_property_enumerator %}
static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    Vector<String> names;
    ExceptionState exceptionState(ExceptionState::EnumerationContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    imp->namedPropertyEnumerator(names, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8::Handle<v8::Array> v8names = v8::Array::New(info.GetIsolate(), names.size());
    for (size_t i = 0; i < names.size(); ++i)
        v8names->Set(v8::Integer::New(info.GetIsolate(), i), v8String(info.GetIsolate(), names[i]));
    v8SetReturnValue(info, v8names);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block named_property_enumerator_callback %}
{% if named_property_getter and named_property_getter.is_enumerable %}
{% set getter = named_property_getter %}
static void namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    {% if getter.is_custom_property_enumerator %}
    {{v8_class}}::namedPropertyEnumeratorCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::namedPropertyEnumerator(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block origin_safe_method_setter %}
{% if has_origin_safe_method_setter %}
static void {{cpp_class}}OriginSafeMethodSetter(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    {# FIXME: don't call GetIsolate 3 times #}
    v8::Handle<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (holder.IsEmpty())
        return;
    {{cpp_class}}* imp = {{v8_class}}::toNative(holder);
    v8::String::Utf8Value attributeName(name);
    ExceptionState exceptionState(ExceptionState::SetterContext, *attributeName, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }

    V8HiddenValue::setHiddenValue(info.GetIsolate(), info.This(), name, jsValue);
}

static void {{cpp_class}}OriginSafeMethodSetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> jsValue, const v8::PropertyCallbackInfo<void>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMSetter");
    {{cpp_class}}V8Internal::{{cpp_class}}OriginSafeMethodSetter(name, jsValue, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8Execution");
}

{% endif %}
{% endblock %}


{##############################################################################}
{% from 'methods.cpp' import named_constructor_callback with context %}
{% block named_constructor %}
{% if named_constructor %}
{% set to_active_dom_object = '%s::toActiveDOMObject' % v8_class
                              if is_active_dom_object else '0' %}
{% set to_event_target = '%s::toEventTarget' % v8_class
                         if is_event_target else '0' %}
const WrapperTypeInfo {{v8_class}}Constructor::wrapperTypeInfo = { gin::kEmbedderBlink, {{v8_class}}Constructor::domTemplate, {{v8_class}}::derefObject, {{to_active_dom_object}}, {{to_event_target}}, 0, {{v8_class}}::installPerContextEnabledMethods, 0, WrapperTypeObjectPrototype, false };

{{named_constructor_callback(named_constructor)}}
v8::Handle<v8::FunctionTemplate> {{v8_class}}Constructor::domTemplate(v8::Isolate* isolate)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> result = data->existingDOMTemplate(&domTemplateKey);
    if (!result.IsEmpty())
        return result;

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    result = v8::FunctionTemplate::New(isolate, {{v8_class}}ConstructorCallback);
    v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
    instanceTemplate->SetInternalFieldCount({{v8_class}}::internalFieldCount);
    result->SetClassName(v8AtomicString(isolate, "{{cpp_class}}"));
    result->Inherit({{v8_class}}::domTemplate(isolate));
    data->setDOMTemplate(&domTemplateKey, result);
    return result;
}

{% endif %}
{% endblock %}

{##############################################################################}
{% block overloaded_constructor %}
{% if constructors|length > 1 %}
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% for constructor in constructors %}
    if ({{constructor.overload_resolution_expression}}) {
        {{cpp_class}}V8Internal::constructor{{constructor.overload_index}}(info);
        return;
    }
    {% endfor %}
    {% if interface_length %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < {{interface_length}})) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments({{interface_length}}, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    exceptionState.throwTypeError("No matching constructor signature.");
    exceptionState.throwIfNeeded();
    {% else %}
    throwTypeError(ExceptionMessages::failedToConstruct("{{interface_name}}", "No matching constructor signature."), info.GetIsolate());
    {% endif %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block event_constructor %}
{% if has_event_constructor %}
static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (info.Length() < 1) {
        exceptionState.throwTypeError("An event name must be provided.");
        exceptionState.throwIfNeeded();
        return;
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, info[0]);
    {% for attribute in any_type_attributes %}
    v8::Local<v8::Value> {{attribute.name}};
    {% endfor %}
    {{cpp_class}}Init eventInit;
    if (info.Length() >= 2) {
        V8TRYCATCH_VOID(Dictionary, options, Dictionary(info[1], info.GetIsolate()));
        if (!initialize{{cpp_class}}(eventInit, options, exceptionState, info)) {
            exceptionState.throwIfNeeded();
            return;
        }
        {# Store attributes of type |any| on the wrapper to avoid leaking them
           between isolated worlds. #}
        {% for attribute in any_type_attributes %}
        options.get("{{attribute.name}}", {{attribute.name}});
        if (!{{attribute.name}}.IsEmpty())
            V8HiddenValue::setHiddenValue(info.GetIsolate(), info.Holder(), v8AtomicString(info.GetIsolate(), "{{attribute.name}}"), {{attribute.name}});
        {% endfor %}
    }
    {% if is_constructor_raises_exception %}
    RefPtr<{{cpp_class}}> event = {{cpp_class}}::create(type, eventInit, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    {% else %}
    RefPtr<{{cpp_class}}> event = {{cpp_class}}::create(type, eventInit);
    {% endif %}
    {% if any_type_attributes and not interface_name == 'ErrorEvent' %}
    {# If we're in an isolated world, create a SerializedScriptValue and store
       it in the event for later cloning if the property is accessed from
       another world. The main world case is handled lazily (in custom code).

       We do not clone Error objects (exceptions), for 2 reasons:
       1) Errors carry a reference to the isolated world's global object, and
          thus passing it around would cause leakage.
       2) Errors cannot be cloned (or serialized):
       http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#safe-passing-of-structured-data #}
    if (DOMWrapperWorld::current(info.GetIsolate())->isIsolatedWorld()) {
        {% for attribute in any_type_attributes %}
        if (!{{attribute.name}}.IsEmpty())
            event->setSerialized{{attribute.name | blink_capitalize}}(SerializedScriptValue::createAndSwallowExceptions({{attribute.name}}, info.GetIsolate()));
        {% endfor %}
    }

    {% endif %}
    v8::Handle<v8::Object> wrapper = info.Holder();
    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(event.release(), &{{v8_class}}::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block visit_dom_wrapper %}
{% if reachable_node_function or set_wrapper_reference_to_list %}
void {{v8_class}}::visitDOMWrapper(void* object, const v8::Persistent<v8::Object>& wrapper, v8::Isolate* isolate)
{
    {{cpp_class}}* impl = fromInternalPointer(object);
    {% if set_wrapper_reference_to_list %}
    v8::Local<v8::Object> creationContext = v8::Local<v8::Object>::New(isolate, wrapper);
    V8WrapperInstantiationScope scope(creationContext, isolate);
    {% for set_wrapper_reference_to in set_wrapper_reference_to_list %}
    {{set_wrapper_reference_to.cpp_type}} {{set_wrapper_reference_to.name}} = impl->{{set_wrapper_reference_to.name}}();
    if ({{set_wrapper_reference_to.name}}) {
        if (!DOMDataStore::containsWrapper<{{set_wrapper_reference_to.v8_type}}>({{set_wrapper_reference_to.name}}, isolate))
            wrap({{set_wrapper_reference_to.name}}, creationContext, isolate);
        DOMDataStore::setWrapperReference<{{set_wrapper_reference_to.v8_type}}>(wrapper, {{set_wrapper_reference_to.name}}, isolate);
    }
    {% endfor %}
    {% endif %}
    {% if reachable_node_function %}
    // The {{reachable_node_function}}() method may return a reference or a pointer.
    if (Node* owner = WTF::getPtr(impl->{{reachable_node_function}}())) {
        Node* root = V8GCController::opaqueRootForGC(owner, isolate);
        isolate->SetReferenceFromGroup(v8::UniqueId(reinterpret_cast<intptr_t>(root)), wrapper);
        return;
    }
    {% endif %}
    setObjectGroup(object, wrapper, isolate);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block shadow_attributes %}
{% if interface_name == 'Window' %}
static const V8DOMConfiguration::AttributeConfiguration shadowAttributes[] = {
    {% for attribute in attributes if attribute.is_unforgeable %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block class_attributes %}
{# FIXME: rename to install_attributes and put into configure_class_template #}
{% if has_attribute_configuration %}
static const V8DOMConfiguration::AttributeConfiguration {{v8_class}}Attributes[] = {
    {% for attribute in attributes
       if not (attribute.is_expose_js_accessors or
               attribute.is_static or
               attribute.runtime_enabled_function or
               attribute.per_context_enabled_function or
               (interface_name == 'Window' and attribute.is_unforgeable)) %}
    {% filter conditional(attribute.conditional_string) %}
    {{attribute_configuration(attribute)}},
    {% endfilter %}
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block class_accessors %}
{# FIXME: rename install_accessors and put into configure_class_template #}
{% if has_accessors %}
static const V8DOMConfiguration::AccessorConfiguration {{v8_class}}Accessors[] = {
    {% for attribute in attributes if attribute.is_expose_js_accessors %}
    {{attribute_configuration(attribute)}},
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block class_methods %}
{# FIXME: rename to install_methods and put into configure_class_template #}
{% if has_method_configuration %}
static const V8DOMConfiguration::MethodConfiguration {{v8_class}}Methods[] = {
    {% for method in methods if method.do_generate_method_configuration %}
    {% filter conditional(method.conditional_string) %}
    {{method_configuration(method)}},
    {% endfilter %}
    {% endfor %}
};

{% endif %}
{% endblock %}


{##############################################################################}
{% block initialize_event %}
{% if has_event_constructor %}
bool initialize{{cpp_class}}({{cpp_class}}Init& eventInit, const Dictionary& options, ExceptionState& exceptionState, const v8::FunctionCallbackInfo<v8::Value>& info, const String& forEventName)
{
    Dictionary::ConversionContext conversionContext(forEventName.isEmpty() ? String("{{interface_name}}") : forEventName, "", exceptionState);
    {% if parent_interface %}{# any Event interface except Event itself #}
    if (!initialize{{parent_interface}}(eventInit, options, exceptionState, info, forEventName.isEmpty() ? String("{{interface_name}}") : forEventName))
        return false;

    {% endif %}
    {% for attribute in attributes
           if (attribute.is_initialized_by_event_constructor and
               not attribute.idl_type == 'any')%}
    {% set is_nullable = 'true' if attribute.is_nullable else 'false' %}
    {% if attribute.deprecate_as %}
    if (options.convert(conversionContext.setConversionType("{{attribute.idl_type}}", {{is_nullable}}), "{{attribute.name}}", eventInit.{{attribute.cpp_name}})) {
        if (options.hasProperty("{{attribute.name}}"))
            UseCounter::countDeprecation(callingExecutionContext(info.GetIsolate()), UseCounter::{{attribute.deprecate_as}});
    } else {
        return false;
    }
    {% else %}
    if (!options.convert(conversionContext.setConversionType("{{attribute.idl_type}}", {{is_nullable}}), "{{attribute.name}}", eventInit.{{attribute.cpp_name}}))
        return false;
    {% endif %}
    {% endfor %}
    return true;
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block constructor_callback %}
{% if constructors or has_custom_constructor or has_event_constructor %}
void {{v8_class}}::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "DOMConstructor");
    {% if measure_as %}
    UseCounter::count(callingExecutionContext(info.GetIsolate()), UseCounter::{{measure_as}});
    {% endif %}
    if (!info.IsConstructCall()) {
        throwTypeError(ExceptionMessages::failedToConstruct("{{interface_name}}", "Please use the 'new' operator, this DOM object constructor cannot be called as a function."), info.GetIsolate());
        return;
    }

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

    {% if has_custom_constructor %}
    {{v8_class}}::constructorCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::constructor(info);
    {% endif %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block configure_shadow_object_template %}
{% if interface_name == 'Window' %}
static void configureShadowObjectTemplate(v8::Handle<v8::ObjectTemplate> templ, v8::Isolate* isolate)
{
    V8DOMConfiguration::installAttributes(templ, v8::Handle<v8::ObjectTemplate>(), shadowAttributes, WTF_ARRAY_LENGTH(shadowAttributes), isolate);

    // Install a security handler with V8.
    templ->SetAccessCheckCallbacks(V8Window::namedSecurityCheckCustom, V8Window::indexedSecurityCheckCustom, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8Window::wrapperTypeInfo)));
    templ->SetInternalFieldCount(V8Window::internalFieldCount);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block configure_class_template %}
{# FIXME: rename to install_dom_template and Install{{v8_class}}DOMTemplate #}
static void configure{{v8_class}}Template(v8::Handle<v8::FunctionTemplate> functionTemplate, v8::Isolate* isolate)
{
    functionTemplate->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
    {% set parent_template =
           'V8%s::domTemplate(isolate)' % parent_interface
           if parent_interface else 'v8::Local<v8::FunctionTemplate>()' %}
    {% if runtime_enabled_function %}
    if (!{{runtime_enabled_function}}())
        defaultSignature = V8DOMConfiguration::installDOMClassTemplate(functionTemplate, "", {{parent_template}}, {{v8_class}}::internalFieldCount, 0, 0, 0, 0, 0, 0, isolate);
    else
    {% endif %}
    {% set runtime_enabled_indent = 4 if runtime_enabled_function else 0 %}
    {% filter indent(runtime_enabled_indent, true) %}
    defaultSignature = V8DOMConfiguration::installDOMClassTemplate(functionTemplate, "{{interface_name}}", {{parent_template}}, {{v8_class}}::internalFieldCount,
        {# Test needed as size 0 constant arrays are not allowed in VC++ #}
        {% set attributes_name, attributes_length =
               ('%sAttributes' % v8_class,
                'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class)
           if has_attribute_configuration else (0, 0) %}
        {% set accessors_name, accessors_length =
               ('%sAccessors' % v8_class,
                'WTF_ARRAY_LENGTH(%sAccessors)' % v8_class)
           if has_accessors else (0, 0) %}
        {% set methods_name, methods_length =
               ('%sMethods' % v8_class,
                'WTF_ARRAY_LENGTH(%sMethods)' % v8_class)
           if has_method_configuration else (0, 0) %}
        {{attributes_name}}, {{attributes_length}},
        {{accessors_name}}, {{accessors_length}},
        {{methods_name}}, {{methods_length}},
        isolate);
    {% endfilter %}

    {% if constructors or has_custom_constructor or has_event_constructor %}
    functionTemplate->SetCallHandler({{v8_class}}::constructorCallback);
    functionTemplate->SetLength({{interface_length}});
    {% endif %}
    v8::Local<v8::ObjectTemplate> ALLOW_UNUSED instanceTemplate = functionTemplate->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> ALLOW_UNUSED prototypeTemplate = functionTemplate->PrototypeTemplate();
    {% if is_check_security and interface_name != 'Window' %}
    instanceTemplate->SetAccessCheckCallbacks({{cpp_class}}V8Internal::namedSecurityCheck, {{cpp_class}}V8Internal::indexedSecurityCheck, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&{{v8_class}}::wrapperTypeInfo)));
    {% endif %}
    {% for attribute in attributes
       if attribute.runtime_enabled_function and
          not attribute.per_context_enabled_function and
          not attribute.is_static %}
    {% filter conditional(attribute.conditional_string) %}
    if ({{attribute.runtime_enabled_function}}()) {
        static const V8DOMConfiguration::AttributeConfiguration attributeConfiguration =\
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAttribute(instanceTemplate, prototypeTemplate, attributeConfiguration, isolate);
    }
    {% endfilter %}
    {% endfor %}
    {% if constants %}
    {{install_constants() | indent}}
    {% endif %}
    {# Special operations #}
    {# V8 has access-check callback API and it's used on Window instead of
       deleters or enumerators; see ObjectTemplate::SetAccessCheckCallbacks.
       In addition, the getter should be set on the prototype template, to get
       the implementation straight out of the Window prototype, regardless of
       what prototype is actually set on the object. #}
    {% set set_on_template = 'PrototypeTemplate' if interface_name == 'Window'
                        else 'InstanceTemplate' %}
    {% if indexed_property_getter %}
    {# if have indexed properties, MUST have an indexed property getter #}
    {% set indexed_property_getter_callback =
           '%sV8Internal::indexedPropertyGetterCallback' % cpp_class %}
    {% set indexed_property_setter_callback =
           '%sV8Internal::indexedPropertySetterCallback' % cpp_class
           if indexed_property_setter else '0' %}
    {% set indexed_property_query_callback = '0' %}{# Unused #}
    {% set indexed_property_deleter_callback =
           '%sV8Internal::indexedPropertyDeleterCallback' % cpp_class
           if indexed_property_deleter else '0' %}
    {% set indexed_property_enumerator_callback =
           'indexedPropertyEnumerator<%s>' % cpp_class
           if indexed_property_getter.is_enumerable else '0' %}
    functionTemplate->{{set_on_template}}()->SetIndexedPropertyHandler({{indexed_property_getter_callback}}, {{indexed_property_setter_callback}}, {{indexed_property_query_callback}}, {{indexed_property_deleter_callback}}, {{indexed_property_enumerator_callback}});
    {% endif %}
    {% if named_property_getter %}
    {# if have named properties, MUST have a named property getter #}
    {% set named_property_getter_callback =
           '%sV8Internal::namedPropertyGetterCallback' % cpp_class %}
    {% set named_property_setter_callback =
           '%sV8Internal::namedPropertySetterCallback' % cpp_class
           if named_property_setter else '0' %}
    {% set named_property_query_callback =
           '%sV8Internal::namedPropertyQueryCallback' % cpp_class
           if named_property_getter.is_enumerable else '0' %}
    {% set named_property_deleter_callback =
           '%sV8Internal::namedPropertyDeleterCallback' % cpp_class
           if named_property_deleter else '0' %}
    {% set named_property_enumerator_callback =
           '%sV8Internal::namedPropertyEnumeratorCallback' % cpp_class
           if named_property_getter.is_enumerable else '0' %}
    functionTemplate->{{set_on_template}}()->SetNamedPropertyHandler({{named_property_getter_callback}}, {{named_property_setter_callback}}, {{named_property_query_callback}}, {{named_property_deleter_callback}}, {{named_property_enumerator_callback}});
    {% endif %}
    {# End special operations #}
    {% if has_custom_legacy_call_as_function %}
    functionTemplate->InstanceTemplate()->SetCallAsFunctionHandler({{v8_class}}::legacyCallCustom);
    {% endif %}
    {% if interface_name == 'HTMLAllCollection' %}
    {# Needed for legacy support of document.all #}
    functionTemplate->InstanceTemplate()->MarkAsUndetectable();
    {% endif %}
    {% for method in methods if not method.do_not_check_signature %}
    {# install_custom_signature #}
    {% if not method.overload_index or method.overload_index == 1 %}
    {# For overloaded methods, only generate one accessor #}
    {% filter conditional(method.conditional_string) %}
    {% if method.is_do_not_check_security %}
    {% if method.is_per_world_bindings %}
    if (DOMWrapperWorld::current(isolate)->isMainWorld()) {
        {{install_do_not_check_security_signature(method, 'ForMainWorld')}}
    } else {
        {{install_do_not_check_security_signature(method)}}
    }
    {% else %}
    {{install_do_not_check_security_signature(method)}}
    {% endif %}
    {% else %}{# is_do_not_check_security #}
    {% if method.is_per_world_bindings %}
    if (DOMWrapperWorld::current(isolate)->isMainWorld()) {
        {% filter runtime_enabled(method.runtime_enabled_function) %}
        {{install_custom_signature(method, 'ForMainWorld')}}
        {% endfilter %}
    } else {
        {% filter runtime_enabled(method.runtime_enabled_function) %}
        {{install_custom_signature(method)}}
        {% endfilter %}
    }
    {% else %}
    {% filter runtime_enabled(method.runtime_enabled_function) %}
    {{install_custom_signature(method)}}
    {% endfilter %}
    {% endif %}
    {% endif %}{# is_do_not_check_security #}
    {% endfilter %}
    {% endif %}{# install_custom_signature #}
    {% endfor %}
    {% for attribute in attributes if attribute.is_static %}
    {% set getter_callback = '%sV8Internal::%sAttributeGetterCallback' %
           (cpp_class, attribute.name) %}
    {% filter conditional(attribute.conditional_string) %}
    functionTemplate->SetNativeDataProperty(v8AtomicString(isolate, "{{attribute.name}}"), {{getter_callback}}, {{attribute.setter_callback}}, v8::External::New(isolate, 0), static_cast<v8::PropertyAttribute>(v8::None), v8::Handle<v8::AccessorSignature>(), static_cast<v8::AccessControl>(v8::DEFAULT));
    {% endfilter %}
    {% endfor %}
    {# Special interfaces #}
    {% if interface_name == 'Window' %}

    prototypeTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    functionTemplate->SetHiddenPrototype(true);
    instanceTemplate->SetInternalFieldCount(V8Window::internalFieldCount);
    // Set access check callbacks, but turned off initially.
    // When a context is detached from a frame, turn on the access check.
    // Turning on checks also invalidates inline caches of the object.
    instanceTemplate->SetAccessCheckCallbacks(V8Window::namedSecurityCheckCustom, V8Window::indexedSecurityCheckCustom, v8::External::New(isolate, const_cast<WrapperTypeInfo*>(&V8Window::wrapperTypeInfo)), false);
    {% elif interface_name in [
           'HTMLDocument', 'DedicatedWorkerGlobalScope',
           'SharedWorkerGlobalScope', 'ServiceWorkerGlobalScope'] %}
    functionTemplate->SetHiddenPrototype(true);
    {% endif %}

    // Custom toString template
    functionTemplate->Set(v8AtomicString(isolate, "toString"), V8PerIsolateData::current()->toStringTemplate());
}

{% endblock %}


{######################################}
{% macro install_do_not_check_security_signature(method, world_suffix) %}
{# FIXME: move to V8DOMConfiguration::installDOMCallbacksWithDoNotCheckSecuritySignature #}
{# Methods that are [DoNotCheckSecurity] are always readable, but if they are
   changed and then accessed from a different origin, we do not return the
   underlying value, but instead return a new copy of the original function.
   This is achieved by storing the changed value as a hidden property. #}
{% set getter_callback =
       '%sV8Internal::%sOriginSafeMethodGetterCallback%s' %
       (cpp_class, method.name, world_suffix) %}
{% set setter_callback =
    '{0}V8Internal::{0}OriginSafeMethodSetterCallback'.format(cpp_class)
    if not method.is_read_only else '0' %}
{% set property_attribute =
    'static_cast<v8::PropertyAttribute>(%s)' %
    ' | '.join(method.property_attributes or ['v8::DontDelete']) %}
{{method.function_template}}->SetAccessor(v8AtomicString(isolate, "{{method.name}}"), {{getter_callback}}, {{setter_callback}}, v8Undefined(), v8::ALL_CAN_READ, {{property_attribute}});
{%- endmacro %}


{######################################}
{% macro install_custom_signature(method, world_suffix) %}
{# FIXME: move to V8DOMConfiguration::installDOMCallbacksWithCustomSignature #}
{% set method_callback = '%sV8Internal::%sMethodCallback%s' %
                         (cpp_class, method.name, world_suffix) %}
{% set property_attribute = 'static_cast<v8::PropertyAttribute>(%s)' %
                            ' | '.join(method.property_attributes) %}
{{method.function_template}}->Set(v8AtomicString(isolate, "{{method.name}}"), v8::FunctionTemplate::New(isolate, {{method_callback}}, v8Undefined(), {{method.signature}}, {{method.number_of_required_or_variadic_arguments}}){% if method.property_attributes %}, {{property_attribute}}{% endif %});
{%- endmacro %}


{######################################}
{% macro install_constants() %}
{# FIXME: should use reflected_name instead of name #}
{# Normal (always enabled) constants #}
static const V8DOMConfiguration::ConstantConfiguration {{v8_class}}Constants[] = {
    {% for constant in constants if not constant.runtime_enabled_function %}
    {"{{constant.name}}", {{constant.value}}},
    {% endfor %}
};
V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, {{v8_class}}Constants, WTF_ARRAY_LENGTH({{v8_class}}Constants), isolate);
{# Runtime-enabled constants #}
{% for constant in constants if constant.runtime_enabled_function %}
if ({{constant.runtime_enabled_function}}()) {
    static const V8DOMConfiguration::ConstantConfiguration constantConfiguration = {"{{constant.name}}", static_cast<signed int>({{constant.value}})};
    V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, &constantConfiguration, 1, isolate);
}
{% endfor %}
{# Check constants #}
{% if not do_not_check_constants %}
{% for constant in constants %}
{% set constant_cpp_class = constant.cpp_class or cpp_class %}
COMPILE_ASSERT({{constant.value}} == {{constant_cpp_class}}::{{constant.reflected_name}}, TheValueOf{{cpp_class}}_{{constant.reflected_name}}DoesntMatchWithImplementation);
{% endfor %}
{% endif %}
{% endmacro %}


{##############################################################################}
{% block get_template %}
{# FIXME: rename to get_dom_template and GetDOMTemplate #}
v8::Handle<v8::FunctionTemplate> {{v8_class}}::domTemplate(v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Local<v8::FunctionTemplate> result = data->existingDOMTemplate(const_cast<WrapperTypeInfo*>(&wrapperTypeInfo));
    if (!result.IsEmpty())
        return result;

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    result = v8::FunctionTemplate::New(isolate, V8ObjectConstructor::isValidConstructorMode);
    configure{{v8_class}}Template(result, isolate);
    data->setDOMTemplate(const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), result);
    return result;
}

{% endblock %}


{##############################################################################}
{% block has_instance %}
bool {{v8_class}}::hasInstance(v8::Handle<v8::Value> jsValue, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&wrapperTypeInfo, jsValue);
}

v8::Handle<v8::Object> {{v8_class}}::findInstanceInPrototypeChain(v8::Handle<v8::Value> jsValue, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->findInstanceInPrototypeChain(&wrapperTypeInfo, jsValue);
}

{% endblock %}


{##############################################################################}
{% block to_native_with_type_check %}
{{cpp_class}}* {{v8_class}}::toNativeWithTypeCheck(v8::Isolate* isolate, v8::Handle<v8::Value> value)
{
    return hasInstance(value, isolate) ? fromInternalPointer(v8::Handle<v8::Object>::Cast(value)->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex)) : 0;
}

{% endblock %}


{##############################################################################}
{% block install_per_context_attributes %}
{% if has_per_context_enabled_attributes %}
void {{v8_class}}::installPerContextEnabledProperties(v8::Handle<v8::Object> instanceTemplate, {{cpp_class}}* impl, v8::Isolate* isolate)
{
    v8::Local<v8::Object> prototypeTemplate = v8::Local<v8::Object>::Cast(instanceTemplate->GetPrototype());
    {% for attribute in attributes if attribute.per_context_enabled_function %}
    if ({{attribute.per_context_enabled_function}}(impl->document())) {
        static const V8DOMConfiguration::AttributeConfiguration attributeConfiguration =\
        {{attribute_configuration(attribute)}};
        V8DOMConfiguration::installAttribute(instanceTemplate, prototypeTemplate, attributeConfiguration, isolate);
    }
    {% endfor %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block install_per_context_methods %}
{% if has_per_context_enabled_methods %}
void {{v8_class}}::installPerContextEnabledMethods(v8::Handle<v8::Object> prototypeTemplate, v8::Isolate* isolate)
{
    {# Define per-context enabled operations #}
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(isolate, domTemplate(isolate));

    ExecutionContext* context = toExecutionContext(prototypeTemplate->CreationContext());
    {% for method in methods if method.per_context_enabled_function %}
    if (context && context->isDocument() && {{method.per_context_enabled_function}}(toDocument(context)))
        prototypeTemplate->Set(v8AtomicString(isolate, "{{method.name}}"), v8::FunctionTemplate::New(isolate, {{cpp_class}}V8Internal::{{method.name}}MethodCallback, v8Undefined(), defaultSignature, {{method.number_of_required_arguments}})->GetFunction());
    {% endfor %}
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block to_active_dom_object %}
{% if is_active_dom_object %}
ActiveDOMObject* {{v8_class}}::toActiveDOMObject(v8::Handle<v8::Object> wrapper)
{
    return toNative(wrapper);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block to_event_target %}
{% if is_event_target %}
EventTarget* {{v8_class}}::toEventTarget(v8::Handle<v8::Object> object)
{
    return toNative(object);
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block get_shadow_object_template %}
{% if interface_name == 'Window' %}
v8::Handle<v8::ObjectTemplate> V8Window::getShadowObjectTemplate(v8::Isolate* isolate)
{
    if (DOMWrapperWorld::current(isolate)->isMainWorld()) {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForMainWorld, ());
        if (V8WindowShadowObjectCacheForMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
            v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate);
            V8WindowShadowObjectCacheForMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForMainWorld);
    } else {
        DEFINE_STATIC_LOCAL(v8::Persistent<v8::ObjectTemplate>, V8WindowShadowObjectCacheForNonMainWorld, ());
        if (V8WindowShadowObjectCacheForNonMainWorld.IsEmpty()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
            v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
            configureShadowObjectTemplate(templ, isolate);
            V8WindowShadowObjectCacheForNonMainWorld.Reset(isolate, templ);
            return templ;
        }
        return v8::Local<v8::ObjectTemplate>::New(isolate, V8WindowShadowObjectCacheForNonMainWorld);
    }
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block wrap %}
{% if special_wrap_for or is_document %}
v8::Handle<v8::Object> wrap({{cpp_class}}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    {% for special_wrap_interface in special_wrap_for %}
    if (impl->is{{special_wrap_interface}}())
        return wrap(to{{special_wrap_interface}}(impl), creationContext, isolate);
    {% endfor %}
    v8::Handle<v8::Object> wrapper = {{v8_class}}::createWrapper(impl, creationContext, isolate);
    {% if is_document %}
    if (wrapper.IsEmpty())
        return wrapper;
    DOMWrapperWorld* world = DOMWrapperWorld::current(isolate);
    if (world->isMainWorld()) {
        if (LocalFrame* frame = impl->frame())
            frame->script().windowShell(world)->updateDocumentWrapper(wrapper);
    }
    {% endif %}
    return wrapper;
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block create_wrapper %}
{% if not has_custom_to_v8 %}
v8::Handle<v8::Object> {{v8_class}}::createWrapper({{pass_ref_ptr}}<{{cpp_class}}> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(!DOMDataStore::containsWrapper<{{v8_class}}>(impl.get(), isolate));
    if (ScriptWrappable::wrapperCanBeStoredInObject(impl.get())) {
        const WrapperTypeInfo* actualInfo = ScriptWrappable::getTypeInfoFromObject(impl.get());
        // Might be a XXXConstructor::wrapperTypeInfo instead of an XXX::wrapperTypeInfo. These will both have
        // the same object de-ref functions, though, so use that as the basis of the check.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(actualInfo->derefObjectFunction == wrapperTypeInfo.derefObjectFunction);
    }

    {% if is_document %}
    if (LocalFrame* frame = impl->frame()) {
        if (frame->script().initializeMainWorld()) {
            // initializeMainWorld may have created a wrapper for the object, retry from the start.
            v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapper<{{v8_class}}>(impl.get(), isolate);
            if (!wrapper.IsEmpty())
                return wrapper;
        }
    }
    {% endif %}
    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &wrapperTypeInfo, toInternalPointer(impl.get()), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    {% if is_audio_buffer %}
    {# We only setDeallocationObservers on array buffers that are held by some
       object in the V8 heap, not in the ArrayBuffer constructor itself.
       This is because V8 GC only cares about memory it can free on GC, and
       until the object is exposed to JavaScript, V8 GC doesn't affect it. #}
    for (unsigned i = 0, n = impl->numberOfChannels(); i < n; i++) {
        Float32Array* channelData = impl->getChannelData(i);
        channelData->buffer()->setDeallocationObserver(V8ArrayBufferDeallocationObserver::instanceTemplate());
    }
    {% endif %}
    installPerContextEnabledProperties(wrapper, impl.get(), isolate);
    {% set wrapper_configuration = 'WrapperConfiguration::Dependent'
                                   if (has_visit_dom_wrapper or
                                       is_active_dom_object or
                                       is_dependent_lifetime) else
                                   'WrapperConfiguration::Independent' %}
    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl, &wrapperTypeInfo, wrapper, isolate, {{wrapper_configuration}});
    return wrapper;
}

{% endif %}
{% endblock %}


{##############################################################################}
{% block deref_object_and_to_v8_no_inline %}
void {{v8_class}}::derefObject(void* object)
{
{% set oilpan_conditional = '!ENABLE(OILPAN)' if is_will_be_garbage_collected
                                              else '' %}
{% filter conditional(oilpan_conditional) %}
    fromInternalPointer(object)->deref();
{% endfilter %}
}

template<>
v8::Handle<v8::Value> toV8NoInline({{cpp_class}}* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    return toV8(impl, creationContext, isolate);
}

{% endblock %}
